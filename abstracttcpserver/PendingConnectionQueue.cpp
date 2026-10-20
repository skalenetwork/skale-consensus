#include "SkaleCommon.h"
#include "PendingConnectionQueue.h"
#include "exceptions/InvalidArgumentException.h"
#include "network/ServerConnection.h"

PendingConnectionQueue::PendingConnectionQueue( size_t _capacity ) : capacity( _capacity ) {
    CHECK_ARGUMENT( capacity > 0 );
}

bool PendingConnectionQueue::tryPush( const ptr< ServerConnection >& _connection ) {
    CHECK_ARGUMENT( _connection );
    {
        lock_guard< mutex > lock( connectionsMutex );
        if ( stopped || connections.size() >= capacity )
            return false;
        connections.push( _connection );
    }
    connectionsCond.notify_one();
    return true;
}

ptr< ServerConnection > PendingConnectionQueue::waitAndPop() {
    unique_lock< mutex > lock( connectionsMutex );
    connectionsCond.wait( lock, [this] { return stopped || !connections.empty(); } );
    if ( stopped )
        return nullptr;

    auto connection = std::move( connections.front() );
    connections.pop();
    return connection;
}

void PendingConnectionQueue::shutdown() {
    {
        lock_guard< mutex > lock( connectionsMutex );
        stopped = true;
    }
    connectionsCond.notify_all();
}
