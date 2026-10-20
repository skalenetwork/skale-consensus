#include "SkaleCommon.h"
#include "tests/e2e/harness/ConsensusProcess.h"
#include "thirdparty/catch.hpp"

#include <filesystem>

CATCH_TEST_CASE( "Consensus supervisor reports child status on failures",
    "[end-to-end][consensus-process][child-status]" ) {
    std::string mode;
    CATCH_SECTION( "Startup error exit" ) { mode = "startup-exit"; }
    CATCH_SECTION( "Startup crash" ) { mode = "startup-signal"; }
    CATCH_SECTION( "Progress error exit" ) { mode = "progress-exit"; }
    CATCH_SECTION( "Progress crash" ) { mode = "progress-signal"; }
    CATCH_SECTION( "Shutdown error exit" ) { mode = "stop-exit"; }
    CATCH_SECTION( "Shutdown crash" ) { mode = "stop-signal"; }
    CATCH_SECTION( "Error exit after shutdown acknowledgment" ) { mode = "stop-ack-exit"; }
    CATCH_SECTION( "Timeout forces termination" ) { mode = "progress-timeout"; }

    std::string failure;
    try {
        E2ETestUtils::ConsensusProcess child{
            CONSENSUS_FAILURE_CHILD_PATH, CONSENSUS_TEST_SOURCE_ROOT, { mode } };
        const auto deadline = E2ETestUtils::deadlineIn(
            mode == "progress-timeout" ? std::chrono::milliseconds( 100 ) :
                                        std::chrono::milliseconds( 2000 ) );
        if ( mode.find( "stop" ) == 0 )
            child.stop( deadline );
        else
            child.progress( deadline );
    } catch ( const std::runtime_error& error ) {
        failure = error.what();
    }
    CATCH_REQUIRE_FALSE( failure.empty() );
    CATCH_REQUIRE( failure.find( "; diagnostics: /tmp/consensus_process_" ) != std::string::npos );
    CATCH_REQUIRE( failure.find( "; cause: " ) != std::string::npos );
    if ( mode == "progress-timeout" ) {
        CATCH_REQUIRE( failure.find( "supervisor-requested SIGKILL" ) != std::string::npos );
        CATCH_REQUIRE( failure.find( "timed out" ) != std::string::npos );
    } else {
        const auto status = mode.find( "signal" ) != std::string::npos ?
            "child terminated by signal " + std::to_string( SIGABRT ) :
            std::string( "child exited with code 7" );
        CATCH_REQUIRE( failure.find( status ) != std::string::npos );
        CATCH_REQUIRE( failure.find( "supervisor-requested SIGKILL" ) == std::string::npos );
    }
    const std::string marker = "; diagnostics: ";
    const auto start = failure.find( marker ) + marker.size();
    const auto end = failure.find( "; cause: ", start );
    CATCH_REQUIRE( end != std::string::npos );
    const auto diagnostics = std::filesystem::path( failure.substr( start, end - start ) );
    CATCH_REQUIRE( std::filesystem::is_directory( diagnostics / "logs" ) );
    // Successful failure-path tests need not leave their expected diagnostic bundles behind.
    std::filesystem::remove_all( diagnostics );
}
