#pragma once

#include <chrono>

namespace E2ETestUtils::HarnessTimeouts {

inline constexpr auto PROGRESS_RESPONSE = std::chrono::seconds( 30 );
inline constexpr auto SHUTDOWN = std::chrono::seconds( 40 );
// Parent readiness includes child startup and the child's first-block wait.
inline constexpr auto PARENT_READINESS = std::chrono::seconds( 60 );
inline constexpr auto CHILD_FIRST_BLOCK = std::chrono::seconds( 45 );
inline constexpr auto CHILD_COMMAND_IDLE = std::chrono::seconds( 120 );
inline constexpr auto CHILD_RESPONSE = std::chrono::seconds( 10 );
inline constexpr auto FAILURE_FIXTURE_EXCHANGE = std::chrono::seconds( 5 );

inline constexpr auto CHILD_READINESS_POLL = std::chrono::milliseconds( 50 );
inline constexpr auto CHILD_REAP_POLL = std::chrono::milliseconds( 10 );

}  // namespace E2ETestUtils::HarnessTimeouts
