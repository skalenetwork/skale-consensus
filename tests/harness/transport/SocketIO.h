#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <thread>

namespace E2ETestUtils {

using Deadline = std::chrono::steady_clock::time_point;

enum class WaitResult { Satisfied, TimedOut };

inline Deadline deadlineIn( std::chrono::milliseconds timeout ) {
    return std::chrono::steady_clock::now() + timeout;
}

// Predicates must bound their own work by deadline; exceptions propagate to the caller.
// Expired deadlines skip evaluation, and results obtained after expiry are not accepted.
template < typename Predicate >
[[nodiscard]] WaitResult waitUntil(
    Predicate predicate, Deadline deadline, std::chrono::milliseconds interval ) {
    if ( interval <= std::chrono::milliseconds::zero() )
        throw std::invalid_argument( "Polling interval must be positive" );
    while ( std::chrono::steady_clock::now() < deadline ) {
        const bool ready = predicate();
        const auto now = std::chrono::steady_clock::now();
        if ( now >= deadline )
            return WaitResult::TimedOut;
        if ( ready )
            return WaitResult::Satisfied;
        std::this_thread::sleep_until( std::min( deadline, now + interval ) );
    }
    return WaitResult::TimedOut;
}

// Sends all size bytes from data by the absolute deadline, or throws after possible partial transfer.
void sendAll( int fd, const void* data, size_t size, Deadline deadline );

// Fills data with exactly size bytes by the absolute deadline, or throws with possible partial writes.
// The caller must provide at least size bytes of writable storage.
void receiveExact( int fd, void* data, size_t size, Deadline deadline );

} // namespace E2ETestUtils
