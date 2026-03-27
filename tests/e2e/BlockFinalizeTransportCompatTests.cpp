/*
    Copyright (C) 2026 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with skale-consensus.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "thirdparty/catch.hpp"
#include "SkaleCommon.h"
#include "node/ConsensusEngine.h"
#include "tests/e2e/ConsensusEngineTestAccess.h"
#include "tests/e2e/E2ETestHelper.h"

namespace BlockFinalizeTransportCompatTests {

using E2EHelper = E2ETestUtils::E2ETestHelper;

static constexpr const char* FINALIZATION_DOWNLOAD_ONLY_ENV =
    "TEST_FINALIZATION_DOWNLOAD_ONLY";
static constexpr useconds_t POLL_INTERVAL_US = 250 * 1000;
static constexpr size_t MAX_POLL_ROUNDS = 20;

struct ScenarioResult {
    block_id lastCommittedBlockId = 0;
    BlockFinalizeTransportStats stats;
};

static ScenarioResult runScenario( const string& _configDir ) {
    ConsensusEngine* testEngine = nullptr;
    auto finalizationDownloadOnlySnapshot =
        TestUtils::setTestEnvVar( FINALIZATION_DOWNLOAD_ONLY_ENV, "1", 1 );

    try {
        E2EHelper::configureTestEnvironment( true, _configDir );
        auto lastId = ( uint64_t ) E2EHelper::startEngineAndWait(
            testEngine, 0, E2EHelper::CROSS_NODE_RUN_TIME_S );

        CATCH_REQUIRE( testEngine != nullptr );
        CATCH_REQUIRE( ( uint64_t ) testEngine->nodesCount() == 2 );
        CATCH_REQUIRE( lastId > 0 );

        auto stats = ConsensusEngineTestAccess::getBlockFinalizeTransportStats( *testEngine );

        CATCH_REQUIRE( stats.totalRequests() > 0 );

        E2EHelper::stopEngineGracefully( testEngine );
        TestUtils::restoreTestEnvVar(
            FINALIZATION_DOWNLOAD_ONLY_ENV, finalizationDownloadOnlySnapshot );

        return { block_id( lastId ), stats };
    } catch ( ... ) {
        if ( testEngine != nullptr ) {
            E2EHelper::stopEngineGracefully( testEngine );
        }
        TestUtils::restoreTestEnvVar(
            FINALIZATION_DOWNLOAD_ONLY_ENV, finalizationDownloadOnlySnapshot );
        throw;
    }
}

}  // namespace BlockFinalizeTransportCompatTests

using namespace BlockFinalizeTransportCompatTests;

CATCH_TEST_CASE( "BlockFinalize uses ZMQ between upgraded nodes",
    "[blockfinalize-transport][end-to-end]" ) {
    auto result = runScenario( "test/twonodes_blockfinalize_both_new_zmq" );

    CATCH_CAPTURE( result.lastCommittedBlockId );
    CATCH_REQUIRE( result.stats.zmqClientAttempts > 0 );
    CATCH_REQUIRE( result.stats.zmqServerRequestsServed > 0 );
    CATCH_REQUIRE( result.stats.zmqClientFallbacksToTcp == 0 );
    CATCH_REQUIRE( result.stats.tcpClientRequests == 0 );
    CATCH_REQUIRE( result.stats.tcpServerRequestsServed == 0 );
}

CATCH_TEST_CASE( "BlockFinalize falls back from new client to old TCP-only server",
    "[blockfinalize-transport][end-to-end]" ) {
    auto result = runScenario( "test/twonodes_blockfinalize_new_client_old_server" );

    CATCH_CAPTURE( result.lastCommittedBlockId );
    CATCH_REQUIRE( result.stats.zmqClientAttempts > 0 );
    CATCH_REQUIRE( result.stats.zmqClientFallbacksToTcp > 0 );
    CATCH_REQUIRE( result.stats.tcpClientRequests > 0 );
    CATCH_REQUIRE( result.stats.tcpServerRequestsServed > 0 );
    CATCH_REQUIRE( result.stats.zmqServerRequestsServed == 0 );
}

CATCH_TEST_CASE( "BlockFinalize keeps legacy TCP path for old client against new server",
    "[blockfinalize-transport][end-to-end]" ) {
    auto result = runScenario( "test/twonodes_blockfinalize_old_client_new_server" );

    CATCH_CAPTURE( result.lastCommittedBlockId );
    CATCH_REQUIRE( result.stats.zmqClientAttempts == 0 );
    CATCH_REQUIRE( result.stats.zmqClientFallbacksToTcp == 0 );
    CATCH_REQUIRE( result.stats.tcpClientRequests > 0 );
    CATCH_REQUIRE( result.stats.tcpServerRequestsServed > 0 );
    CATCH_REQUIRE( result.stats.zmqServerRequestsServed == 0 );
}
