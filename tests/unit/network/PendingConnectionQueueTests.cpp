#include "SkaleCommon.h"
#include "abstracttcpserver/PendingConnectionQueue.h"
#include "network/ServerConnection.h"
#include "tests/e2e/harness/TestSockets.h"
#include "thirdparty/catch.hpp"

#include <cerrno>
#include <fcntl.h>
#include <future>
#include <memory>
#include <vector>

namespace {
struct ShutdownQueueOnExit {
    PendingConnectionQueue& queue;

    ~ShutdownQueueOnExit() {
        queue.shutdown();
    }
};

void requireClosed( int fd ) {
    const int result = ::fcntl( fd, F_GETFD );
    const int error = errno;
    CATCH_REQUIRE( result == -1 );
    CATCH_REQUIRE( error == EBADF );
}

ptr< ServerConnection > createConnection() {
    return std::make_shared< ServerConnection >(
        E2ETestUtils::createTestSocket( AF_INET, SOCK_STREAM ), "127.0.0.1" );
}
}  // namespace

CATCH_TEST_CASE( "Pending TCP queue enforces its capacity and releases connections",
    "[unit][network][pending-queue]" ) {
    static_assert( PendingConnectionQueue::DEFAULT_CAPACITY == 1024,
        "Pending TCP admission must retain its default 1024 boundary" );
    constexpr size_t capacity = 3;
    const auto initialObjects = ServerConnection::getTotalObjects();

    std::vector< int > queuedDescriptors;
    queuedDescriptors.reserve( capacity );
    {
        PendingConnectionQueue queue( capacity );

        // Fill the queue to capacity and confirm it retains every admitted socket.
        for ( size_t i = 0; i < capacity; ++i ) {
            auto connection = createConnection();
            queuedDescriptors.push_back( int( connection->getDescriptor() ) );
            CATCH_REQUIRE( queue.tryPush( connection ) );
        }
        CATCH_REQUIRE( ServerConnection::getTotalObjects() == initialObjects + capacity );
        for ( int fd : queuedDescriptors )
            CATCH_REQUIRE( ::fcntl( fd, F_GETFD ) >= 0 );

        int rejectedDescriptor = -1;
        std::weak_ptr< ServerConnection > rejected;
        {
            auto connection = createConnection();
            rejectedDescriptor = int( connection->getDescriptor() );
            rejected = connection;
            CATCH_REQUIRE_FALSE( queue.tryPush( connection ) );
            CATCH_REQUIRE( ::fcntl( rejectedDescriptor, F_GETFD ) >= 0 );
        }
        CATCH_REQUIRE( rejected.expired() );
        requireClosed( rejectedDescriptor );
        CATCH_REQUIRE( ServerConnection::getTotalObjects() == initialObjects + capacity );

        // Free one slot and confirm a replacement connection can be admitted.
        const int removedDescriptor = queuedDescriptors.front();
        {
            auto removed = queue.waitAndPop();
            CATCH_REQUIRE( removed );
            CATCH_REQUIRE( int( removed->getDescriptor() ) == removedDescriptor );
            CATCH_REQUIRE( ::fcntl( removedDescriptor, F_GETFD ) >= 0 );
        }
        requireClosed( removedDescriptor );
        CATCH_REQUIRE( ServerConnection::getTotalObjects() == initialObjects + capacity - 1 );
        {
            auto replacement = createConnection();
            CATCH_REQUIRE( queue.tryPush( replacement ) );
            queuedDescriptors.front() = int( replacement->getDescriptor() );
        }
        CATCH_REQUIRE( ServerConnection::getTotalObjects() == initialObjects + capacity );
        auto overflow = createConnection();
        CATCH_REQUIRE_FALSE( queue.tryPush( overflow ) );
    }
    // Confirm queue destruction releases all remaining connections and sockets.
    CATCH_REQUIRE( ServerConnection::getTotalObjects() == initialObjects );
    for ( int fd : queuedDescriptors )
        requireClosed( fd );
}

CATCH_TEST_CASE( "Pending TCP queue dequeues connections in FIFO order",
    "[unit][network][pending-queue]" ) {
    PendingConnectionQueue queue;
    auto first = createConnection();
    auto second = createConnection();
    CATCH_REQUIRE( queue.tryPush( first ) );
    CATCH_REQUIRE( queue.tryPush( second ) );
    CATCH_REQUIRE( queue.waitAndPop() == first );
    CATCH_REQUIRE( queue.waitAndPop() == second );
}

CATCH_TEST_CASE( "Pending TCP queue wakes a consumer when a connection arrives",
    "[unit][network][pending-queue]" ) {
    PendingConnectionQueue queue;
    auto consumer = std::async( std::launch::async, [&queue] { return queue.waitAndPop(); } );
    ShutdownQueueOnExit cleanup{ queue };
    CATCH_REQUIRE( consumer.wait_for( std::chrono::milliseconds( 50 ) ) ==
                   std::future_status::timeout );

    auto connection = createConnection();
    CATCH_REQUIRE( queue.tryPush( connection ) );
    CATCH_REQUIRE( consumer.wait_for( std::chrono::seconds( 1 ) ) == std::future_status::ready );
    CATCH_REQUIRE( consumer.get() == connection );
}

CATCH_TEST_CASE( "Pending TCP queue shutdown wakes all consumers and prevents further use",
    "[unit][network][pending-queue]" ) {
    PendingConnectionQueue queue;
    std::vector< std::future< ptr< ServerConnection > > > consumers;
    ShutdownQueueOnExit cleanup{ queue };
    for ( size_t i = 0; i < 3; ++i )
        consumers.push_back(
            std::async( std::launch::async, [&queue] { return queue.waitAndPop(); } ) );
    for ( auto& consumer : consumers )
        CATCH_REQUIRE( consumer.wait_for( std::chrono::milliseconds( 50 ) ) ==
                       std::future_status::timeout );

    queue.shutdown();
    queue.shutdown();
    for ( auto& consumer : consumers ) {
        CATCH_REQUIRE( consumer.wait_for( std::chrono::seconds( 1 ) ) ==
                       std::future_status::ready );
        CATCH_REQUIRE_FALSE( consumer.get() );
    }
    CATCH_REQUIRE_FALSE( queue.waitAndPop() );
    auto connection = createConnection();
    CATCH_REQUIRE_FALSE( queue.tryPush( connection ) );
}

CATCH_TEST_CASE( "Pending TCP queue shutdown retains queued ownership until destruction",
    "[unit][network][pending-queue]" ) {
    std::weak_ptr< ServerConnection > pending;
    int descriptor = -1;
    {
        PendingConnectionQueue queue;
        {
            auto connection = createConnection();
            descriptor = int( connection->getDescriptor() );
            pending = connection;
            CATCH_REQUIRE( queue.tryPush( connection ) );
        }
        queue.shutdown();
        CATCH_REQUIRE_FALSE( queue.waitAndPop() );
        CATCH_REQUIRE_FALSE( pending.expired() );
        CATCH_REQUIRE( ::fcntl( descriptor, F_GETFD ) >= 0 );
    }
    CATCH_REQUIRE( pending.expired() );
    requireClosed( descriptor );
}

CATCH_TEST_CASE( "Pending TCP queue rejects null connections",
    "[unit][network][pending-queue]" ) {
    PendingConnectionQueue queue;
    CATCH_REQUIRE_THROWS( queue.tryPush( nullptr ) );
}

CATCH_TEST_CASE( "Pending TCP queue rejects zero capacity",
    "[unit][network][pending-queue]" ) {
    CATCH_REQUIRE_THROWS( PendingConnectionQueue( 0 ) );
}
