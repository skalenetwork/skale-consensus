#pragma once

namespace E2ETestUtils::HarnessDiagnostics {

inline constexpr char CHILD_EXIT_CODE[] = "child exited with code ";
inline constexpr char CHILD_SIGNAL[] = "child terminated by signal ";
inline constexpr char SUPERVISOR_KILL[] = " (supervisor-requested SIGKILL)";
inline constexpr char DIAGNOSTICS_MARKER[] = "; diagnostics: ";
inline constexpr char CAUSE_MARKER[] = "; cause: ";
inline constexpr char SOCKET_TIMEOUT[] = "Consensus test socket operation timed out";
inline constexpr char CONTROL_CHANNEL_CLOSED[] = "Consensus child control channel is closed";

}  // namespace E2ETestUtils::HarnessDiagnostics
