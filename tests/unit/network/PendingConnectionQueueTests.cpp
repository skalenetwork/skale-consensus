#include "SkaleCommon.h"
#include "abstracttcpserver/AbstractServerAgent.h"
#include "network/ServerConnection.h"
#include "tests/e2e/harness/TestSockets.h"
#include "thirdparty/catch.hpp"

#include <cerrno>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace {
// Exercises production admission without a node or workers; exposes queue inspection and
// test-controlled removal, not production dequeue or concurrent synchronization.
class QueueTestServer final : public AbstractServerAgent {
public:
    using AbstractServerAgent::MAX_PENDING_CONNECTIONS;

    QueueTestServer() : AbstractServerAgent( "QueueTestServer" ) {}

    size_t pendingCount() {
        std::lock_guard< std::mutex > lock( incomingTCPConnectionsMutex );
        return incomingTCPConnections.size();
    }

    void removeOnePending() {
        std::lock_guard< std::mutex > lock( incomingTCPConnectionsMutex );
        if ( incomingTCPConnections.empty() )
            throw std::logic_error( "Expected a pending connection" );
        incomingTCPConnections.pop();
    }

    void processNextAvailableConnection( const ptr< ServerConnection >& ) override {
        throw std::logic_error( "Queue test must not start workers" );
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

CATCH_TEST_CASE( "Pending TCP queue enforces the 1024 boundary and releases connections",
    "[component][network][pending-queue]" ) {
    constexpr size_t capacity = QueueTestServer::MAX_PENDING_CONNECTIONS;
    const auto initialObjects = ServerConnection::getTotalObjects();

    std::vector< int > queuedDescriptors;
    queuedDescriptors.reserve( capacity );
    {
        QueueTestServer server;
        CATCH_REQUIRE( server.pendingCount() == 0 );
        
        // Fill the queue to capacity and confirm it retains every admitted socket.
        for ( size_t i = 0; i < capacity; ++i ) {
            auto connection = createConnection();
            queuedDescriptors.push_back( int( connection->getDescriptor() ) );
            CATCH_REQUIRE( server.tryPushToQueueAndNotifyWorker( connection ) );
        }
        CATCH_REQUIRE( server.pendingCount() == capacity );
        CATCH_REQUIRE( ServerConnection::getTotalObjects() == initialObjects + capacity );
        for ( int fd : queuedDescriptors )
            CATCH_REQUIRE( ::fcntl( fd, F_GETFD ) >= 0 );

        // Confirm full-queue rejection closes each socket once the caller releases ownership.
        for ( size_t attempt = 0; attempt < 16; ++attempt ) {
            int rejectedDescriptor = -1;
            std::weak_ptr< ServerConnection > rejected;
            {
                auto connection = createConnection();
                rejectedDescriptor = int( connection->getDescriptor() );
                rejected = connection;
                CATCH_REQUIRE_FALSE( server.tryPushToQueueAndNotifyWorker( connection ) );
                CATCH_REQUIRE( server.pendingCount() == capacity );
                // Rejection leaves cleanup to the accept loop's owning reference.
                CATCH_REQUIRE( ::fcntl( rejectedDescriptor, F_GETFD ) >= 0 );
            }
            CATCH_REQUIRE( rejected.expired() );
            requireClosed( rejectedDescriptor );
            CATCH_REQUIRE( ServerConnection::getTotalObjects() == initialObjects + capacity );
        }

        // Free one slot and confirm a replacement connection can be admitted.
        const int removedDescriptor = queuedDescriptors.front();
        server.removeOnePending();
        CATCH_REQUIRE( server.pendingCount() == capacity - 1 );
        requireClosed( removedDescriptor );
        CATCH_REQUIRE( ServerConnection::getTotalObjects() == initialObjects + capacity - 1 );
        {
            auto replacement = createConnection();
            CATCH_REQUIRE( server.tryPushToQueueAndNotifyWorker( replacement ) );
            queuedDescriptors.front() = int( replacement->getDescriptor() );
        }
        CATCH_REQUIRE( server.pendingCount() == capacity );
        CATCH_REQUIRE( ServerConnection::getTotalObjects() == initialObjects + capacity );
    }
    // Confirm queue destruction releases all remaining connections and sockets.
    CATCH_REQUIRE( ServerConnection::getTotalObjects() == initialObjects );
    for ( int fd : queuedDescriptors )
        requireClosed( fd );
}
