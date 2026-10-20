#include "SkaleCommon.h"
#include "network/ServerConnection.h"
#include "network/TCPServerSocket.h"
#include "tests/TestHooks.h"
#include "tests/e2e/harness/ConsensusProcess.h"
#include "tests/e2e/harness/TestSockets.h"
#include "thirdparty/catch.hpp"

#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <limits>
#include <thread>
#include <type_traits>

namespace {
class FailureChildExecutable {
    std::filesystem::path directory;

public:
    explicit FailureChildExecutable( const std::string& mode ) {
        char temporary[] = "/tmp/consensus_failure_fixture_XXXXXX";
        const auto created = ::mkdtemp( temporary );
        if ( created == nullptr )
            throw std::runtime_error( "Could not create failure fixture directory" );
        directory = created;
        try {
            std::filesystem::create_symlink( CONSENSUS_FAILURE_CHILD_PATH, directory / mode );
        } catch ( ... ) {
            removeDirectory();
            throw;
        }
    }
    ~FailureChildExecutable() { removeDirectory(); }
    FailureChildExecutable( const FailureChildExecutable& ) = delete;
    FailureChildExecutable& operator=( const FailureChildExecutable& ) = delete;

    std::string path( const std::string& mode ) const { return ( directory / mode ).string(); }

private:
    void removeDirectory() noexcept {
        std::error_code error;
        std::filesystem::remove_all( directory, error );
        if ( error )
            std::fprintf( stderr, "Could not remove failure fixture directory: %s\n",
                error.message().c_str() );
    }
};
}

CATCH_TEST_CASE( "Consensus supervisor reports child status on failures",
    "[e2e][consensus-process][child-status]" ) {
    std::string mode;
    CATCH_SECTION( "Startup error exit" ) { mode = "startup-exit"; }
    CATCH_SECTION( "Startup crash" ) { mode = "startup-signal"; }
    CATCH_SECTION( "Progress error exit" ) { mode = "progress-exit"; }
    CATCH_SECTION( "Progress crash" ) { mode = "progress-signal"; }
    CATCH_SECTION( "Shutdown error exit" ) { mode = "stop-exit"; }
    CATCH_SECTION( "Shutdown crash" ) { mode = "stop-signal"; }
    CATCH_SECTION( "Error exit after shutdown acknowledgment" ) { mode = "stop-ack-exit"; }
    CATCH_SECTION( "Timeout forces termination" ) { mode = "progress-timeout"; }

    FailureChildExecutable executable{ mode };
    std::string failure;
    try {
        E2ETestUtils::ConsensusProcess child{ executable.path( mode ), CONSENSUS_TEST_SOURCE_ROOT };
        const auto deadline = std::chrono::steady_clock::now() +
            ( mode == "progress-timeout" ? std::chrono::milliseconds( 100 ) :
                                          std::chrono::milliseconds( 2000 ) );
        if ( mode.find( "stop" ) == 0 )
            child.stop( deadline );
        else
            child.progress( deadline );
    } catch ( const std::runtime_error& error ) {
        failure = error.what();
    }
    CATCH_REQUIRE_FALSE( failure.empty() );
    CATCH_REQUIRE( failure.find( "; diagnostics: /tmp/consensus_process_" ) != std::string::npos );
    CATCH_REQUIRE( failure.find( "; cause: " ) != std::string::npos );
    if ( mode == "progress-timeout" ) {
        CATCH_REQUIRE( failure.find( "supervisor-requested SIGKILL" ) != std::string::npos );
        CATCH_REQUIRE( failure.find( "timed out" ) != std::string::npos );
    } else {
        const auto status = mode.find( "signal" ) != std::string::npos ?
            "child terminated by signal " + std::to_string( SIGABRT ) :
            std::string( "child exited with code 7" );
        CATCH_REQUIRE( failure.find( status ) != std::string::npos );
        CATCH_REQUIRE( failure.find( "supervisor-requested SIGKILL" ) == std::string::npos );
    }
    const std::string marker = "; diagnostics: ";
    const auto start = failure.find( marker ) + marker.size();
    const auto end = failure.find( "; cause: ", start );
    CATCH_REQUIRE( end != std::string::npos );
    const auto diagnostics = std::filesystem::path( failure.substr( start, end - start ) );
    CATCH_REQUIRE( std::filesystem::is_directory( diagnostics / "logs" ) );
    // Successful failure-path tests need not leave their expected diagnostic bundles behind.
    std::filesystem::remove_all( diagnostics );
}

CATCH_TEST_CASE( "Scoped sockets transfer ownership and close on scope exit",
    "[e2e][consensus-process][socket]" ) {
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
    "[e2e][consensus-process][socket]" ) {
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
    "[e2e][consensus-process][socket]" ) {
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
    "[e2e][consensus-process][socket]" ) {
    Socket invalid( std::numeric_limits< int >::max() );
    errno = EINVAL;
    const int closeError = invalid.reset();
    CATCH_REQUIRE( errno == EINVAL );
    CATCH_REQUIRE( closeError == EBADF );
    CATCH_REQUIRE_FALSE( invalid );
    CATCH_REQUIRE( invalid.reset() == 0 );
}

CATCH_TEST_CASE( "Server connections own their sockets until destruction",
    "[e2e][consensus-process][socket]" ) {
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
    "[e2e][consensus-process][socket]" ) {
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
    "[e2e][consensus-process][socket]" ) {
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

CATCH_TEST_CASE( "Consensus control tags round trip and reject unknown values",
    "[e2e][consensus-process][protocol]" ) {
    using namespace E2ETestUtils;
    CATCH_REQUIRE( std::string( toString( ControlCommand::Progress ) ) == "progress" );
    CATCH_REQUIRE( std::string( toString( ControlCommand::Stop ) ) == "stop" );
    CATCH_REQUIRE( std::string( toString( ControlEvent::Ready ) ) == "ready" );
    CATCH_REQUIRE( std::string( toString( ControlEvent::Progress ) ) == "progress" );
    CATCH_REQUIRE( std::string( toString( ControlEvent::Stopped ) ) == "stopped" );
    for ( auto command : { ControlCommand::Progress, ControlCommand::Stop,
             ControlCommand::InjectAcceptError, ControlCommand::ClearAcceptError,
             ControlCommand::AcceptErrorCount } )
        CATCH_REQUIRE( controlCommandFromString( toString( command ) ) == command );
    for ( auto event : { ControlEvent::Ready, ControlEvent::Progress, ControlEvent::Stopped,
             ControlEvent::AcceptErrorEnabled, ControlEvent::AcceptErrorCleared,
             ControlEvent::AcceptErrorCount } )
        CATCH_REQUIRE( controlEventFromString( toString( event ) ) == event );
    CATCH_REQUIRE_THROWS_AS( controlCommandFromString( "unknown" ), std::invalid_argument );
    CATCH_REQUIRE_THROWS_AS( controlEventFromString( "unknown" ), std::invalid_argument );
    CATCH_REQUIRE_THROWS_AS( controlCommandFromString( "" ), std::invalid_argument );
    CATCH_REQUIRE_THROWS_AS( controlEventFromString( "" ), std::invalid_argument );
}

CATCH_TEST_CASE( "Control channels exchange typed commands and JSON responses",
    "[e2e][consensus-process][protocol]" ) {
    using namespace E2ETestUtils;
    auto sockets = createTestSocketPair();
    ControlChannel parent{ std::move( sockets[0] ) };
    ControlChannel child{ std::move( sockets[1] ) };
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( 1 );

    child.sendResponse( ControlEvent::Ready, { { "height", uint64_t{ 1 } } }, deadline );
    CATCH_REQUIRE( parent.receiveResponse( ControlEvent::Ready, deadline ).at( "height" ) == 1 );
    parent.sendCommand( ControlCommand::Progress, deadline );
    CATCH_REQUIRE( child.receiveCommand( deadline ) == ControlCommand::Progress );
    child.sendResponse( ControlEvent::Progress, { { "height", uint64_t{ 2 } } }, deadline );
    CATCH_REQUIRE( parent.receiveResponse( ControlEvent::Progress, deadline ).at( "height" ) == 2 );
    parent.sendCommand( ControlCommand::Stop, deadline );
    CATCH_REQUIRE( child.receiveCommand( deadline ) == ControlCommand::Stop );
    child.sendResponse( ControlEvent::Stopped, nlohmann::json::object(), deadline );
    CATCH_REQUIRE( parent.receiveResponse( ControlEvent::Stopped, deadline ).size() == 1 );
}

CATCH_TEST_CASE( "Control channels reject invalid responses and cannot be reused",
    "[e2e][consensus-process][protocol]" ) {
    using namespace E2ETestUtils;
    auto sockets = createTestSocketPair();
    ControlChannel channel{ std::move( sockets[0] ) };
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( 1 );
    std::string body;
    uint32_t size = 0;

    CATCH_SECTION( "Empty frame" ) {}
    CATCH_SECTION( "Oversized frame" ) { size = 4097; }
    CATCH_SECTION( "Malformed JSON" ) { body = "{"; }
    CATCH_SECTION( "Missing event" ) { body = "{}"; }
    CATCH_SECTION( "Unknown event" ) { body = R"({"event":"unknown"})"; }
    CATCH_SECTION( "Unexpected event" ) { body = R"({"event":"stopped"})"; }

    if ( !body.empty() )
        size = static_cast< uint32_t >( body.size() );
    sendAll( sockets[1].get(), &size, sizeof( size ), deadline );
    if ( !body.empty() )
        sendAll( sockets[1].get(), body.data(), body.size(), deadline );
    CATCH_REQUIRE_THROWS( channel.receiveResponse( ControlEvent::Progress, deadline ) );
    CATCH_REQUIRE_THROWS_WITH( channel.sendCommand( ControlCommand::Progress, deadline ),
        "Consensus child control channel is closed" );
}

CATCH_TEST_CASE( "Control channels close after incomplete frames reach their deadline",
    "[e2e][consensus-process][protocol][deadline]" ) {
    using namespace E2ETestUtils;
    auto sockets = createTestSocketPair();
    ControlChannel channel{ std::move( sockets[0] ) };
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds( 50 );
    const uint32_t size = 8;
    sendAll( sockets[1].get(), &size, sizeof( size ), deadline );
    sendAll( sockets[1].get(), "pro", 3, deadline );
    CATCH_REQUIRE_THROWS_WITH( channel.receiveCommand( deadline ),
        "Consensus test socket operation timed out" );
    CATCH_REQUIRE_THROWS_WITH( channel.receiveCommand( deadline ),
        "Consensus child control channel is closed" );
}

CATCH_TEST_CASE( "Control channels reject unknown commands",
    "[e2e][consensus-process][protocol]" ) {
    using namespace E2ETestUtils;
    auto sockets = createTestSocketPair();
    ControlChannel channel{ std::move( sockets[0] ) };
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( 1 );
    const std::string body = "unknown";
    const auto size = static_cast< uint32_t >( body.size() );
    sendAll( sockets[1].get(), &size, sizeof( size ), deadline );
    sendAll( sockets[1].get(), body.data(), body.size(), deadline );
    CATCH_REQUIRE_THROWS_WITH( channel.receiveCommand( deadline ), "Unknown parent command: unknown" );
    CATCH_REQUIRE_THROWS_WITH( channel.receiveCommand( deadline ),
        "Consensus child control channel is closed" );
}

CATCH_TEST_CASE( "Control responses cannot reset an exchange deadline",
    "[e2e][consensus-process][protocol][deadline]" ) {
    using namespace E2ETestUtils;
    auto sockets = createTestSocketPair();
    ControlChannel parent{ std::move( sockets[0] ) };
    ControlChannel child{ std::move( sockets[1] ) };
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds( 50 );
    parent.sendCommand( ControlCommand::Progress, deadline );
    CATCH_REQUIRE( child.receiveCommand( deadline ) == ControlCommand::Progress );
    child.sendResponse( ControlEvent::Progress, { { "height", uint64_t{ 1 } } }, deadline );
    std::this_thread::sleep_until( deadline );
    CATCH_REQUIRE_THROWS_WITH( parent.receiveResponse( ControlEvent::Progress, deadline ),
        "Consensus test socket operation timed out" );
    CATCH_REQUIRE_THROWS_WITH( parent.sendCommand( ControlCommand::Progress, deadline ),
        "Consensus child control channel is closed" );
}

CATCH_TEST_CASE( "Consensus socket transfers respect shared deadlines",
    "[e2e][consensus-process][deadline]" ) {
    using E2ETestUtils::sendAll;
    using E2ETestUtils::receiveExact;
    auto sockets = E2ETestUtils::createTestSocketPair();
    char sent = 'x';
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds( 50 );
    sendAll( sockets[0].get(), &sent, sizeof( sent ), deadline );

    CATCH_SECTION( "An expired deadline rejects a subsequent transfer even with data ready" ) {
        std::this_thread::sleep_until( deadline );
        char received = '\0';
        CATCH_REQUIRE_THROWS_WITH(
            receiveExact( sockets[1].get(), &received, sizeof( received ), deadline ),
            "Consensus test socket operation timed out" );
        CATCH_REQUIRE( received == '\0' );
    }

    CATCH_SECTION( "A partial frame cannot extend the deadline" ) {
        char received[2]{};
        CATCH_REQUIRE_THROWS_WITH(
            receiveExact( sockets[1].get(), received, sizeof( received ), deadline ),
            "Consensus test socket operation timed out" );
        CATCH_REQUIRE( received[0] == sent );
        CATCH_REQUIRE( std::chrono::steady_clock::now() < deadline + std::chrono::seconds( 1 ) );
    }
}
