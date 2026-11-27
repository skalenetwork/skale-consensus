#include "SkaleCommon.h"
#include "fixtures/SupervisorFailureModes.h"
#include "tests/harness/process/ConsensusProcess.h"
#include "tests/harness/HarnessDiagnostics.h"
#include "thirdparty/catch.hpp"

#include <filesystem>

CATCH_TEST_CASE( "Harness supervisor reports child status on failures correctly",
    "[end-to-end][harness][consensus-process][child-status]" ) {

    using namespace SupervisorFailureFixture;
    namespace Diagnostics = E2ETestUtils::HarnessDiagnostics;

    for ( const auto& mode : failureModes ) {
        CATCH_DYNAMIC_SECTION( mode.name ) {

            std::string failure;
            bool ready = false;
            E2ETestUtils::ConsensusProcess child{
                SUPERVISOR_FAILURE_CHILD_PATH, CONSENSUS_TEST_SOURCE_ROOT, { mode.name } };
            try {
                // will fail on purpose on some failure modes
                child.start();
                ready = true;
                CATCH_REQUIRE_THROWS_AS( child.start(), std::logic_error );

                const auto deadline = E2ETestUtils::deadlineIn(
                    mode.outcome == FailureOutcome::Timeout ? std::chrono::milliseconds( 100 ) :
                                                              std::chrono::milliseconds( 2000 ) );

                if ( mode.stage == FailureStage::Shutdown ||
                     mode.stage == FailureStage::ShutdownAcknowledged ) {
                    child.stop( deadline );
                }
                else {
                    child.progress( deadline );
                }
            } catch ( const std::runtime_error& error ) {
                // the process should always enter here
                failure = error.what();
            }

            // process should always fail with some error message
            CATCH_REQUIRE_FALSE( failure.empty() );
            CATCH_REQUIRE( ready == ( mode.stage != FailureStage::Startup ) );

            // can't start again after failure
            CATCH_REQUIRE_THROWS_AS( child.start(), std::logic_error );
            CATCH_REQUIRE_THROWS_AS( child.progress(), std::logic_error );
            CATCH_REQUIRE_THROWS_AS( child.stop(), std::logic_error );
            CATCH_REQUIRE(
                failure.find( std::string( Diagnostics::DIAGNOSTICS_MARKER ) +
                    child.logsDirectory().parent_path().string() ) != std::string::npos );
            CATCH_REQUIRE( failure.find( Diagnostics::CAUSE_MARKER ) != std::string::npos );

            if ( mode.outcome == FailureOutcome::Timeout ) {
                CATCH_REQUIRE(
                    failure.find( Diagnostics::SUPERVISOR_KILL ) != std::string::npos );
                CATCH_REQUIRE( failure.find( Diagnostics::SOCKET_TIMEOUT ) != std::string::npos );
            } else {
                const auto status = mode.outcome == FailureOutcome::Signal ?
                    Diagnostics::CHILD_SIGNAL + std::to_string( SIGABRT ) :
                    Diagnostics::CHILD_EXIT_CODE + std::to_string( FAILURE_EXIT_CODE );
                CATCH_REQUIRE( failure.find( status ) != std::string::npos );
                CATCH_REQUIRE(
                    failure.find( Diagnostics::SUPERVISOR_KILL ) == std::string::npos );
            }

            const std::string marker = Diagnostics::DIAGNOSTICS_MARKER;
            const auto start = failure.find( marker ) + marker.size();
            const auto end = failure.find( Diagnostics::CAUSE_MARKER, start );
            CATCH_REQUIRE( end != std::string::npos );
            const auto diagnostics =
                std::filesystem::path( failure.substr( start, end - start ) );
            CATCH_REQUIRE( std::filesystem::is_directory( diagnostics / "logs" ) );
            // Successful failure-path tests need not leave their expected diagnostic bundles behind.
            std::filesystem::remove_all( diagnostics );

        }
    }
}
