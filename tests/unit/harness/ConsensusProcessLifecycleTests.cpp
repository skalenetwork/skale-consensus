#include "tests/harness/process/ConsensusProcess.h"
#include "thirdparty/catch.hpp"

#include <stdexcept>

CATCH_TEST_CASE( "Unstarted consensus supervisor rejects commands",
    "[unit][harness][consensus-process][lifecycle]" ) {
    using namespace E2ETestUtils;
    ConsensusProcess child{ "/nonexistent/consensus-child", "/nonexistent/consensus-source" };
    CATCH_REQUIRE( child.address().empty() );
    CATCH_REQUIRE_THROWS_AS( child.progress(), std::logic_error );
    CATCH_REQUIRE_THROWS_AS( child.stop(), std::logic_error );
    CATCH_REQUIRE_THROWS_AS(
        child.injectAcceptError( deadlineIn( HarnessTimeouts::PROGRESS_RESPONSE ) ),
        std::logic_error );
    CATCH_REQUIRE_THROWS_AS(
        child.clearAcceptError( deadlineIn( HarnessTimeouts::PROGRESS_RESPONSE ) ),
        std::logic_error );
    CATCH_REQUIRE_THROWS_AS(
        child.acceptErrorCount( deadlineIn( HarnessTimeouts::PROGRESS_RESPONSE ) ),
        std::logic_error );
}
