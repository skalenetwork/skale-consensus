#pragma once

#include "SkaleCommon.h"

class ServerConnection;

// Bounded, nonblocking admission with blocking FIFO consumption.
// Shutdown stops consumption without draining; queued connections live until destruction.
// Owners must shut down and join consumers before destroying the queue.
class PendingConnectionQueue {
public:
    static constexpr size_t DEFAULT_CAPACITY = 1024;

    explicit PendingConnectionQueue( size_t _capacity = DEFAULT_CAPACITY );

    // Returns false when full or stopped; rejection leaves ownership with the caller.
    bool tryPush( const ptr< ServerConnection >& _connection );

    // Returns nullptr after shutdown, including when connections remain queued.
    ptr< ServerConnection > waitAndPop();

    void shutdown();

private:
    const size_t capacity;
    mutex connectionsMutex;
    condition_variable connectionsCond;
    queue< ptr< ServerConnection > > connections;
    bool stopped = false;
};
