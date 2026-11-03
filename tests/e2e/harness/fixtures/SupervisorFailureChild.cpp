#include "tests/harness/transport/ControlChannel.h"
#include "tests/harness/HarnessTimeouts.h"
#include "SupervisorFailureModes.h"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <sys/resource.h>
#include <unistd.h>

namespace {
using namespace SupervisorFailureFixture;

[[noreturn]] void fail( FailureOutcome outcome ) {
    switch ( outcome ) {
        case FailureOutcome::Signal:
            std::abort();
        case FailureOutcome::Exit:
            ::_exit( FAILURE_EXIT_CODE );
        case FailureOutcome::Timeout:
            for ( ;; )
                ::pause();
    }
    throw std::invalid_argument( "Invalid supervisor failure outcome" );
}
}

// Fake child for supervisor failure tests; never starts a consensus engine.
int main( int argc, char** argv ) {
    try {
        if ( argc != 4 )
            throw std::invalid_argument( "Expected configuration and data directories and failure mode" );
        const rlimit noCore{ 0, 0 };

        if ( ::setrlimit( RLIMIT_CORE, &noCore ) != 0 )
            throw std::runtime_error( "Could not disable fixture core dumps" );
        const auto& mode = parseFailureMode( argv[3] );

        // Fail immediately if the failure stage is Startup.
        if ( mode.stage == FailureStage::Startup )
            fail( mode.outcome );

        using namespace E2ETestUtils;
        ControlChannel control{ Socket{ STDIN_FILENO } };

        // simulate the child is ready
        control.sendResponse(
            ControlEvent::Ready,
            { { "height", uint64_t{ 1 } } },
            deadlineIn( HarnessTimeouts::FAILURE_FIXTURE_EXCHANGE )
        );

        // determine the expected command based on the failure stage
        const auto expectedCommand = mode.stage == FailureStage::Progress ?
            ControlCommand::Progress : ControlCommand::Stop;

        // wait for the supervisor to send the next command
        const ControlCommand command = control.receiveCommand(
            deadlineIn( HarnessTimeouts::FAILURE_FIXTURE_EXCHANGE ) );

        if ( command != expectedCommand ) {
            throw std::runtime_error(
                std::string( "Expected control command: " ) + toString( expectedCommand ) );
        }

        if ( mode.stage == FailureStage::ShutdownAcknowledged ) {
            control.sendResponse( ControlEvent::Stopped, nlohmann::json::object(),
                deadlineIn( HarnessTimeouts::FAILURE_FIXTURE_EXCHANGE ) );
        }

        fail( mode.outcome );
    } catch ( const std::exception& error ) {
        std::fprintf( stderr, "Supervisor failure fixture failed: %s\n", error.what() );
        return 2;
    }
}
