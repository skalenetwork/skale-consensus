#pragma once

#include <chrono>
#include <cstddef>

namespace E2ETestUtils {

using Deadline = std::chrono::steady_clock::time_point;

// Sends all size bytes from data by the absolute deadline, or throws after possible partial transfer.
void sendAll( int fd, const void* data, size_t size, Deadline deadline );

// Fills data with exactly size bytes by the absolute deadline, or throws with possible partial writes.
// The caller must provide at least size bytes of writable storage.
void receiveExact( int fd, void* data, size_t size, Deadline deadline );

} // namespace E2ETestUtils
