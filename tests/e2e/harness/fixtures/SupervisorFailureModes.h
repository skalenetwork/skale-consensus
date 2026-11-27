#pragma once

#include <array>
#include <stdexcept>
#include <string>
#include <string_view>

namespace SupervisorFailureFixture {

enum class FailureStage { Startup, Progress, Shutdown, ShutdownAcknowledged };
enum class FailureOutcome { Exit, Signal, Timeout };

struct FailureMode {
    const char* name;
    FailureStage stage;
    FailureOutcome outcome;
};

inline constexpr int FAILURE_EXIT_CODE = 1;

inline constexpr std::array< FailureMode, 8 > failureModes{ {
    // Supervisor                         Fake child
    //     | -------- launch -----------> |
    //     |                              X exit(FAILURE_EXIT_CODE), no Ready
    //     | report startup failure       |
    { "startup-exit", FailureStage::Startup, FailureOutcome::Exit },

    // Supervisor                         Fake child
    //     | -------- launch -----------> |
    //     |                              X SIGABRT, no Ready
    //     | report startup crash         |
    { "startup-signal", FailureStage::Startup, FailureOutcome::Signal },

    // Supervisor                         Fake child
    //     | <-------- Ready ------------ |
    //     | -------- Progress ---------> |
    //     |                              X exit(FAILURE_EXIT_CODE), no reply
    //     | report progress failure      |
    { "progress-exit", FailureStage::Progress, FailureOutcome::Exit },

    // Supervisor                         Fake child
    //     | <-------- Ready ------------ |
    //     | -------- Progress ---------> |
    //     |                              X SIGABRT, no reply
    //     | report progress crash        |
    { "progress-signal", FailureStage::Progress, FailureOutcome::Signal },

    // Supervisor                         Fake child
    //     | <-------- Ready ------------ |
    //     | -------- Stop -------------> |
    //     |                              X exit(FAILURE_EXIT_CODE), no Stopped
    //     | report shutdown failure      |
    { "stop-exit", FailureStage::Shutdown, FailureOutcome::Exit },

    // Supervisor                         Fake child
    //     | <-------- Ready ------------ |
    //     | -------- Stop -------------> |
    //     |                              X SIGABRT, no Stopped
    //     | report shutdown crash        |
    { "stop-signal", FailureStage::Shutdown, FailureOutcome::Signal },

    // Supervisor                         Fake child
    //     | <-------- Ready ------------ |
    //     | -------- Stop -------------> |
    //     | <-------- Stopped ---------- |
    //     |                              X exit(FAILURE_EXIT_CODE)
    //     | report failure despite ack   |
    { "stop-ack-exit", FailureStage::ShutdownAcknowledged, FailureOutcome::Exit },

    // Supervisor                         Fake child
    //     | <-------- Ready ------------ |
    //     | -------- Progress ---------> |
    //     | wait until deadline          | hang, no reply
    //     | -------- SIGKILL (OS) ------> X
    //     | report timeout + forced kill |
    { "progress-timeout", FailureStage::Progress, FailureOutcome::Timeout },
} };

inline const FailureMode& parseFailureMode( std::string_view name ) {
    for ( const auto& mode : failureModes )
        if ( name == mode.name )
            return mode;
    throw std::invalid_argument( "Unknown supervisor failure mode: " + std::string( name ) );
}

}  // namespace SupervisorFailureFixture
