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

    @author SKALE Labs
    @date 2026
*/

#ifdef BITE2

#include "thirdparty/catch.hpp"
#include "Consensust.h"
#include "SkaleCommon.h"
#include "datastructures/CommittedBlock.h"
#include "node/ConsensusEngine.h"
#include "utils/Time.h"
#include "tests/TestUtils.h"

#include <boost/filesystem.hpp>
#include <cstdlib>
#include <fstream>
#include <map>
#include <unordered_set>
#include <vector>
#include "tests/e2e/E2ETestHelper.h"

namespace RandomTests {

using E2EHelper = E2ETestUtils::E2ETestHelper;

CATCH_TEST_CASE(
    "reencryption random/signature transition across bite2 patch timestamp",
    "[reencryption-signature-transition][bite2][end-to-end][db]" ) {
    
    ConsensusEngine* testEngine = nullptr;
    
    try {
        E2EHelper::configureTestEnvironment( true, "test/twonodes_sameip" );

        auto snap = E2EHelper::setTestEnvVar("TEST_TRANSACTIONS_PER_BLOCK", "1");

        uint64_t runTimeS = Consensust::getRunningTimeS();
        if ( runTimeS < 8 ) {
            runTimeS = 8;
        }

        // 4s before moving into patch to give enough time to have blocks prior the patch as well as after
        static constexpr uint64_t PATCH_DELAY_MS = 4000;
        auto bite2PatchTimestamp = ( Time::getCurrentTimeMs() + PATCH_DELAY_MS + 999 ) / 1000;
        
        testEngine = new ConsensusEngine( 0, 1000000000 );
        testEngine->setTestPatchTimestamps( { { "bite2PatchTimestamp", bite2PatchTimestamp } } );
        testEngine->parseTestConfigsAndCreateAllNodes( Consensust::getConfigDirPath(), false );
        testEngine->slowStartBootStrapTest();
        
        // Wait for blocks to be committed
        usleep( 1000 * 1000 * runTimeS );
        
        auto lastId = testEngine->getLargestCommittedBlockIDInDb();
        
        // Poll briefly if DB commit is still pending
        for ( int i = 0; i < 20 && lastId == 0; i++ ) {
            usleep( 250 * 1000 );
            lastId = testEngine->getLargestCommittedBlockIDInDb();
        }
        
        CATCH_REQUIRE( lastId > 0 );
        CATCH_REQUIRE( ( uint64_t ) testEngine->nodesCount() == 2 );
        
        uint64_t checkedBlocks = 0;
        
        // Access node IDs to validate both node views for each committed block.
        auto nodeIds = testEngine->getNodeIDs();
        CATCH_REQUIRE( nodeIds.size() == 2 );
        auto nodeIt = nodeIds.begin();
        auto firstNodeId = *nodeIt;
        ++nodeIt;
        auto secondNodeId = *nodeIt;
        
        uint64_t checkedBeforePatch = 0;
        uint64_t checkedAfterPatch = 0;

        // Validate transition by block timestamp: 
        // before patch -> absent, 
        // after patch -> present.
        for ( uint64_t blockId = 1; blockId <= (uint64_t)lastId; blockId++ ) {
            try {
                auto firstBlock =
                    testEngine->getCommittedBlockForBlockIdForNode( blockId, firstNodeId );
                auto secondBlock =
                    testEngine->getCommittedBlockForBlockIdForNode( blockId, secondNodeId );
                CATCH_REQUIRE( firstBlock != nullptr );
                CATCH_REQUIRE( secondBlock != nullptr );

                auto firstTimestamp = firstBlock->getTimeStampS();
                auto secondTimestamp = secondBlock->getTimeStampS();
                CATCH_REQUIRE( firstTimestamp == secondTimestamp );

                auto checkNodeForBlock = [&]( node_id _nodeId, const ptr< CommittedBlock >& _block ) {
                    auto blockTimestamp = _block->getTimeStampS();
                    auto reencryptionSignature = _block->getReencryptionThresholdSig();

                    CATCH_INFO( "blockId=" << blockId << ", nodeId=" << _nodeId <<
                        ", blockTimestamp=" << blockTimestamp <<
                        ", bite2PatchTimestamp=" << bite2PatchTimestamp );

                    if ( blockTimestamp < bite2PatchTimestamp ) {
                        bool noReencryptionSignature =
                            !reencryptionSignature.has_value() || reencryptionSignature->empty();
                        CATCH_REQUIRE( noReencryptionSignature );

                        bool randomReadFailed = false;
                        try {
                            auto random =
                                testEngine->getReencryptionRandomForBlockIdForNode( blockId, _nodeId );
                            ( void ) random;
                        } catch ( ... ) {
                            randomReadFailed = true;
                        }
                        CATCH_REQUIRE( randomReadFailed );
                    } else {
                        CATCH_REQUIRE( reencryptionSignature.has_value() );
                        CATCH_REQUIRE( !reencryptionSignature->empty() );
                        auto reencryptionRandom =
                            testEngine->getReencryptionRandomForBlockIdForNode( blockId, _nodeId );
                        CATCH_REQUIRE( reencryptionRandom != u256( 0 ) );
                    }
                };

                checkNodeForBlock( firstNodeId, firstBlock );
                checkNodeForBlock( secondNodeId, secondBlock );

                if ( firstTimestamp < bite2PatchTimestamp ) {
                    checkedBeforePatch++;
                } else {
                    checkedAfterPatch++;
                }
                checkedBlocks++;
            } catch ( const SkaleException& e ) {
                CATCH_FAIL( "Exception thrown while checking block ID " + std::to_string( blockId ) +
                    " for reencryption random/signature transition: " + e.what() );
            } catch ( ... ) {
                CATCH_FAIL( "Unknown exception thrown while checking block ID " +
                    std::to_string( blockId ) + " for reencryption random/signature transition." );
            }
        }
        
        CATCH_REQUIRE( checkedBlocks == lastId );
        CATCH_REQUIRE( checkedBeforePatch > 0 );
        CATCH_REQUIRE( checkedAfterPatch > 0 );
        
        E2EHelper::stopEngineGracefully( testEngine );

        E2EHelper::restoreTestEnvVar( "TEST_TRANSACTIONS_PER_BLOCK", snap );
        
    } catch ( SkaleException& e ) {
        if ( testEngine ) {
            E2EHelper::stopEngineGracefully( testEngine );
        }
        SkaleException::logNested( e );
        throw;
    } catch ( ... ) {
        if ( testEngine ) {
            E2EHelper::stopEngineGracefully( testEngine );
        }
        throw;
    }
    
    CATCH_SUCCEED();
}

} // namespace RandomTests

#endif  // BITE2
