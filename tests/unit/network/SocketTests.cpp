#include "SkaleCommon.h"
#include "network/ServerConnection.h"
#include "network/TCPServerSocket.h"
#include "tests/TestHooks.h"
#include "tests/harness/transport/TestSockets.h"
#include "thirdparty/catch.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <limits>
#include <type_traits>

CATCH_TEST_CASE( "Scoped sockets transfer ownership and close on scope exit",
    "[unit][network][socket]" ) {
    static_assert( !std::is_copy_constructible< Socket >::value );
    static_assert( !std::is_copy_assignable< Socket >::value );
    static_assert( std::is_nothrow_move_constructible< Socket >::value );
    static_assert( std::is_nothrow_move_assignable< Socket >::value );

    auto sockets = E2ETestUtils::createTestSocketPair();
    const int original = sockets[0].get();
    const int replaced = sockets[1].get();
    const int flags = ::fcntl( original, F_GETFD );
    CATCH_REQUIRE( flags >= 0 );
    CATCH_REQUIRE( ( flags & FD_CLOEXEC ) != 0 );
    {
        Socket moved( std::move( sockets[0] ) );
        CATCH_REQUIRE_FALSE( sockets[0] );
        CATCH_REQUIRE( moved.get() == original );
        sockets[1] = std::move( moved );
        CATCH_REQUIRE_FALSE( moved );
        CATCH_REQUIRE( sockets[1].get() == original );
        CATCH_REQUIRE( ::fcntl( replaced, F_GETFD ) == -1 );
        CATCH_REQUIRE( errno == EBADF );
    }
    {
        Socket owner( std::move( sockets[1] ) );
        CATCH_REQUIRE( ::fcntl( owner.get(), F_GETFD ) >= 0 );
    }
    CATCH_REQUIRE( ::fcntl( original, F_GETFD ) == -1 );
    CATCH_REQUIRE( errno == EBADF );
}

CATCH_TEST_CASE( "Scoped sockets reset safely and clean up during unwinding",
    "[unit][network][socket]" ) {
    Socket empty;
    CATCH_REQUIRE_FALSE( empty );
    CATCH_REQUIRE( empty.get() == -1 );
    CATCH_REQUIRE( empty.reset() == 0 );

    auto socket = E2ETestUtils::createTestSocket( AF_INET, SOCK_STREAM | SOCK_NONBLOCK );
    const int descriptor = socket.get();
    CATCH_REQUIRE( socket.reset() == 0 );
    CATCH_REQUIRE_FALSE( socket );
    CATCH_REQUIRE( ::fcntl( descriptor, F_GETFD ) == -1 );
    CATCH_REQUIRE( errno == EBADF );
    CATCH_REQUIRE( socket.reset() == 0 );

    int unwoundDescriptor = -1;
    CATCH_REQUIRE_THROWS_WITH(
        [&]() {
            auto owned = E2ETestUtils::createTestSocket( AF_INET, SOCK_STREAM );
            unwoundDescriptor = owned.get();
            throw std::runtime_error( "Unwind socket owner" );
        }(),
        "Unwind socket owner" );
    CATCH_REQUIRE( unwoundDescriptor >= 0 );
    CATCH_REQUIRE( ::fcntl( unwoundDescriptor, F_GETFD ) == -1 );
    CATCH_REQUIRE( errno == EBADF );
}

CATCH_TEST_CASE( "Socket release transfers ownership without closing",
    "[unit][network][socket]" ) {
    Socket receiver;
    int descriptor = -1;
    {
        auto owner = E2ETestUtils::createTestSocket( AF_INET, SOCK_STREAM );
        descriptor = owner.get();
        receiver = Socket( owner.release() );
        CATCH_REQUIRE_FALSE( owner );
        CATCH_REQUIRE( receiver.get() == descriptor );
    }
    CATCH_REQUIRE( ::fcntl( descriptor, F_GETFD ) >= 0 );
    errno = EINVAL;
    const int closeError = receiver.reset();
    CATCH_REQUIRE( errno == EINVAL );
    CATCH_REQUIRE( closeError == 0 );
    CATCH_REQUIRE( ::fcntl( descriptor, F_GETFD ) == -1 );
    CATCH_REQUIRE( errno == EBADF );
}

CATCH_TEST_CASE( "Socket reset reports close errors without changing errno",
    "[unit][network][socket]" ) {
    Socket invalid( std::numeric_limits< int >::max() );
    errno = EINVAL;
    const int closeError = invalid.reset();
    CATCH_REQUIRE( errno == EINVAL );
    CATCH_REQUIRE( closeError == EBADF );
    CATCH_REQUIRE_FALSE( invalid );
    CATCH_REQUIRE( invalid.reset() == 0 );
}

CATCH_TEST_CASE( "Server connections own their sockets until destruction",
    "[unit][network][socket]" ) {
    auto sockets = E2ETestUtils::createTestSocketPair();
    const int descriptor = sockets[0].get();
    const auto before = ServerConnection::getTotalObjects();
    {
        ServerConnection connection{ std::move( sockets[0] ), "127.0.0.1" };
        CATCH_REQUIRE_FALSE( sockets[0] );
        CATCH_REQUIRE( int( connection.getDescriptor() ) == descriptor );
        CATCH_REQUIRE( ServerConnection::getTotalObjects() == before + 1 );
        CATCH_REQUIRE( ::fcntl( descriptor, F_GETFD ) >= 0 );
    }
    CATCH_REQUIRE( ServerConnection::getTotalObjects() == before );
    CATCH_REQUIRE( ::fcntl( descriptor, F_GETFD ) == -1 );
    CATCH_REQUIRE( errno == EBADF );
}

CATCH_TEST_CASE( "TCP listeners close their owned descriptor exactly once",
    "[unit][network][socket]" ) {
    int descriptor = -1;
    Socket replacement;
    {
        TCPServerSocket listener{ "127.0.0.1", 0, port_type::PROPOSAL };
        descriptor = listener.getDescriptor();
        CATCH_REQUIRE( ::fcntl( descriptor, F_GETFD ) >= 0 );

        CATCH_SECTION( "Destruction closes the listener" ) {}

        CATCH_SECTION( "Repeated cleanup and destruction leave other sockets open" ) {
            listener.closeAndCleanupAll();
            CATCH_REQUIRE( ::fcntl( descriptor, F_GETFD ) == -1 );
            CATCH_REQUIRE( errno == EBADF );
            CATCH_REQUIRE_THROWS( listener.getDescriptor() );

            replacement = E2ETestUtils::createTestSocket( AF_INET, SOCK_STREAM );
            listener.closeAndCleanupAll();
            CATCH_REQUIRE( ::fcntl( replacement.get(), F_GETFD ) >= 0 );
        }
    }
    if ( replacement ) {
        CATCH_REQUIRE( ::fcntl( replacement.get(), F_GETFD ) >= 0 );
    } else {
        CATCH_REQUIRE( ::fcntl( descriptor, F_GETFD ) == -1 );
        CATCH_REQUIRE( errno == EBADF );
    }
}

CATCH_TEST_CASE( "TCP accept error injection is scoped to a listener and preserves real acceptance",
    "[unit][network][socket]" ) {
    TCPServerSocket listener{ "127.0.0.1", 0, port_type::PROPOSAL };
    TCPServerSocket other{ "127.0.0.1", 0, port_type::PROPOSAL };
    const auto connectTo = []( TCPServerSocket& server ) {
        sockaddr_in address{};
        socklen_t length = sizeof( address );
        CATCH_REQUIRE( ::getsockname( server.getDescriptor(),
            reinterpret_cast< sockaddr* >( &address ), &length ) == 0 );
        auto client = E2ETestUtils::createTestSocket( AF_INET, SOCK_STREAM | SOCK_NONBLOCK );
        const int result = ::connect( client.get(),
            reinterpret_cast< sockaddr* >( &address ), length );
        if ( result < 0 )
            CATCH_REQUIRE( errno == EINPROGRESS );
        return client;
    };
    int injectedError = EMFILE;
    CATCH_SECTION( "Process descriptor exhaustion" ) { injectedError = EMFILE; }
    CATCH_SECTION( "System descriptor exhaustion" ) { injectedError = ENFILE; }
    CATCH_SECTION( "Socket buffer exhaustion" ) { injectedError = ENOBUFS; }
    CATCH_SECTION( "Kernel memory exhaustion" ) { injectedError = ENOMEM; }
    size_t failures = 0;
    {
        TestHooks::TCPAcceptErrorHook::Scope hook(
            [&]( const TCPServerSocket& target, int& error ) {
                if ( &target == &listener ) {
                    error = injectedError;
                    ++failures;
                }
            } );
        for ( size_t attempt = 0; attempt < 3; ++attempt ) {
            const auto result = listener.acceptConnection();
            CATCH_REQUIRE( result.status == TCPServerSocket::AcceptStatus::ResourceExhausted );
            CATCH_REQUIRE_FALSE( result.connection );
            CATCH_REQUIRE( result.error == injectedError );
        }
        auto client = connectTo( other );
        const auto result = other.acceptConnection();
        CATCH_REQUIRE( result.status == TCPServerSocket::AcceptStatus::Accepted );
        CATCH_REQUIRE( result.connection );
        CATCH_REQUIRE( failures == 3 );
    }
    CATCH_REQUIRE_FALSE( TestHooks::TCPAcceptErrorHook::isEnabled() );
    auto client = connectTo( listener );
    const auto recovered = listener.acceptConnection();
    CATCH_REQUIRE( recovered.status == TCPServerSocket::AcceptStatus::Accepted );
    CATCH_REQUIRE( recovered.connection );
    CATCH_REQUIRE( recovered.error == 0 );
}
