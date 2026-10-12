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

#ifdef BITE

#include "thirdparty/catch.hpp"
#include "Consensust.h"
#include "SkaleCommon.h"
#include "crypto/AESKeyDecryptionShareList.h"
#include "crypto/ConsensusAESKeyDecryptionShare.h"
#include "db/TEDecryptionDB.h"
#include "node/ConsensusEngine.h"
#include "tests/TestHooks.h"
#include "tests/e2e/ConsensusEngineTestAccess.h"
#include "tests/e2e/E2ETestHelper.h"

#include <libBLS/backends/algebra.hpp>
#include <libBLS/threshold_encryption/TEDecryptionShare.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

namespace ByzantineDecryptionShareTests {

using E2EHelper = E2ETestUtils::E2ETestHelper;

/**
 * @brief Constructs a mutated decryption share list matching structural dimensions
 * but containing semantically invalid decryption shares.
 *
 * Each share is constructed as a mathematically valid G2 curve point (syntactically valid),
 * so it passes deserialization and enters TEDecryptionDB, but fails threshold recovery.
 */
static std::shared_ptr<AESKeyDecryptionShareList> createMutatedShareList(
    const std::shared_ptr<AESKeyDecryptionShareList>& originalShares ) {
    if ( !originalShares || originalShares->size() == 0 ) {
        return originalShares;
    }

    auto mutatedList = std::make_shared<AESKeyDecryptionShareList>(
        originalShares->getBlockId(),
        originalShares->getProposerIndex(),
        originalShares->getDecryptorIndex() );

    for ( const auto& [txIdx, shareVec] : originalShares->getDecryptionShares() ) {
        auto mutatedVec = std::make_shared<AESKeyDecryptionShares>();
        if ( shareVec ) {
            for ( const auto& share : *shareVec ) {
                if ( share ) {
                    // Generate a valid G2 curve point that passes point validation
                    libBLS::algebra::G2Point randomG2 = libBLS::algebra::G2Point::random();
                    randomG2.validate();

                    auto teShare = std::make_shared<libBLS::TEDecryptionShare>(
                        randomG2, (uint64_t) share->getDecryptorIndex(), true );
                    auto mutatedShare = std::make_shared<ConsensusAESKeyDecryptionShare>(
                        teShare, share->getDecryptorIndex(), share->isDecryptionFailed() );
                    mutatedVec->push_back( mutatedShare );
                }
            }
        }
        mutatedList->addShares( txIdx, mutatedVec );
    }

    return mutatedList;
}

struct ScenarioObservability {
    std::atomic<uint32_t> mutatedRecipients{0};
    std::atomic<uint32_t> emptyByzantineRecipients{0};
    std::atomic<uint32_t> withheldRecipients{0};
    std::atomic<uint32_t> releasedRecipients{0};
};

CATCH_TEST_CASE(
    "Byzantine decryption share stall during block finalization download",
    "[bite][byzantine][decryption-share][finalization-stall][regression][sgx][end-to-end]" ) {

    ConsensusEngine* testEngine = nullptr;
    auto envDownloadOnly = E2EHelper::setTestEnvVar( "TEST_FINALIZATION_DOWNLOAD_ONLY", "1" );
    auto envTransactionCount = E2EHelper::setTestEnvVar( "TEST_TRANSACTIONS_PER_BLOCK", "8" );

    std::atomic<bool> releaseHeldShares{false};

    try {
        // Stage 1: Configure test environment and set up network hooks.
        // Node 2 (1113) is Byzantine and sends corrupted shares.
        // A circular honest pair (4->1, 1->3, 3->4) is withheld to force each honest node
        // to collect Node 2's invalid share along with one honest foreign share.
        E2EHelper::configureTestEnvironment( true, "test/fournodes-same-ip" );

        // The three honest nodes need three valid shares from the four decryptors.
        const node_id NODE_1( 1112 );
        const node_id BYZANTINE_NODE( 1113 );
        const node_id NODE_3( 1114 );
        const node_id NODE_4( 1115 );
        constexpr uint32_t NODE_1_BIT = 1u << 0;
        constexpr uint32_t NODE_3_BIT = 1u << 1;
        constexpr uint32_t NODE_4_BIT = 1u << 2;
        constexpr uint32_t ALL_HONEST = NODE_1_BIT | NODE_3_BIT | NODE_4_BIT;

        ScenarioObservability obs;

        auto recipientBit = [&]( node_id receiver ) -> uint32_t {
            if ( receiver == NODE_1 ) return NODE_1_BIT;
            if ( receiver == NODE_3 ) return NODE_3_BIT;
            if ( receiver == NODE_4 ) return NODE_4_BIT;
            return 0;
        };

        // Determine if all decryption shares for Block 1 from 'sender' should be
        // temporarily withheld from 'receiver'.
        auto shouldWithhold = [&]( node_id sender, node_id receiver ) {
            return ( sender == NODE_4 && receiver == NODE_1 ) ||
                   ( sender == NODE_1 && receiver == NODE_3 ) ||
                   ( sender == NODE_3 && receiver == NODE_4 );
        };

        auto responseHook = [&]( node_id senderNodeId,
                                 node_id receiverNodeId,
                                 block_id blockId,
                                 std::shared_ptr<AESKeyDecryptionShareList>& shares,
                                 bool& retryLater ) {
            if ( blockId != 1 ) return;

            const uint32_t bit = recipientBit( receiverNodeId );
            if ( bit == 0 ) return;

            if ( senderNodeId == BYZANTINE_NODE ) {
                if ( !shares || shares->totalCiphertextSharesCount() == 0 ) {
                    obs.emptyByzantineRecipients.fetch_or( bit );
                    return;
                }
                shares = createMutatedShareList( shares );
                obs.mutatedRecipients.fetch_or( bit );
            } else if ( shouldWithhold( senderNodeId, receiverNodeId ) ) {
                if ( !releaseHeldShares.load() ) {
                    obs.withheldRecipients.fetch_or( bit );
                    retryLater = true;
                } else {
                    obs.releasedRecipients.fetch_or( bit );
                }
            }
        };

        // A committed block from another node must not make this finalization test pass via catchup.
        auto catchupHook = []( node_id, node_id, block_id afterBlockId, bool& suppress ) {
            if ( afterBlockId == 0 ) suppress = true;
        };

        TestHooks::BlockFinalizeResponseHook::Scope responseScope( responseHook );
        TestHooks::BlockCatchupResponseHook::Scope catchupScope( catchupHook );

        // Stage 2: Start consensus engine and bootstrap nodes.
        testEngine = new ConsensusEngine( 0, 1000000000 );
        testEngine->parseTestConfigsAndCreateAllNodes( Consensust::getConfigDirPath(), false );

        // Mock crypto cannot reject a corrupted share, so without SGX the recovery below proves
        // nothing. parseTestConfigsAndCreateAllNodes() silently falls back to mock crypto when
        // run_sgx_test/sgx_data is missing, so fail explicitly instead of passing vacuously.
        for ( const auto& nodeId : { NODE_1, BYZANTINE_NODE, NODE_3, NODE_4 } ) {
            CATCH_INFO( "SGX test keys are required (run_sgx_test/sgx_data); node " << nodeId
                                                                              << " uses mock crypto" );
            CATCH_REQUIRE( ConsensusEngineTestAccess::getNode( *testEngine, nodeId )->isSgxEnabled() );
        }

        testEngine->slowStartBootStrapTest();

        auto node1Db = ConsensusEngineTestAccess::getNode( *testEngine, NODE_1 )->getTEDecryptionDB();
        auto node3Db = ConsensusEngineTestAccess::getNode( *testEngine, NODE_3 )->getTEDecryptionDB();
        auto node4Db = ConsensusEngineTestAccess::getNode( *testEngine, NODE_4 )->getTEDecryptionDB();
        CATCH_REQUIRE( node1Db != nullptr );
        CATCH_REQUIRE( node3Db != nullptr );
        CATCH_REQUIRE( node4Db != nullptr );

        auto firstThreePresent = [&] {
            return node1Db->haveDecryptionShares( 1, 1 ) &&
                   node1Db->haveDecryptionShares( 1, 2 ) &&
                   node1Db->haveDecryptionShares( 1, 3 ) &&
                   node3Db->haveDecryptionShares( 1, 2 ) &&
                   node3Db->haveDecryptionShares( 1, 3 ) &&
                   node3Db->haveDecryptionShares( 1, 4 ) &&
                   node4Db->haveDecryptionShares( 1, 1 ) &&
                   node4Db->haveDecryptionShares( 1, 2 ) &&
                   node4Db->haveDecryptionShares( 1, 4 );
        };

        // Stage 3: Wait for all honest nodes to receive their own share, Node 2's mutated
        // share, and one honest foreign share, while the remaining honest share is withheld.
        // Verify honest nodes cannot commit Block 1 (stall due to corrupt share).
        const auto stageDeadline = std::chrono::steady_clock::now() + std::chrono::seconds( 30 );
        while ( std::chrono::steady_clock::now() < stageDeadline ) {
            if ( firstThreePresent() &&
                 obs.mutatedRecipients.load() == ALL_HONEST &&
                 obs.withheldRecipients.load() == ALL_HONEST ) {
                break;
            }
            std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
        }

        CATCH_REQUIRE( obs.emptyByzantineRecipients.load() == 0 );
        CATCH_REQUIRE( obs.mutatedRecipients.load() == ALL_HONEST );
        CATCH_REQUIRE( obs.withheldRecipients.load() == ALL_HONEST );
        CATCH_REQUIRE( firstThreePresent() );

        CATCH_REQUIRE( ConsensusEngineTestAccess::getCurrentLastCommittedBlockIDForNode(
            *testEngine, NODE_1 ) == 0 );
        CATCH_REQUIRE( ConsensusEngineTestAccess::getCurrentLastCommittedBlockIDForNode(
            *testEngine, NODE_3 ) == 0 );
        CATCH_REQUIRE( ConsensusEngineTestAccess::getCurrentLastCommittedBlockIDForNode(
            *testEngine, NODE_4 ) == 0 );

        // Stage 4: Release the withheld honest shares. A correct finalizer must download
        // the missing honest share, discard the Byzantine share, and commit Block 1.
        releaseHeldShares.store( true );

        auto honestNodesCommitted = [&] {
            return ConsensusEngineTestAccess::getCurrentLastCommittedBlockIDForNode(
                       *testEngine, NODE_1 ) >= 1 &&
                   ConsensusEngineTestAccess::getCurrentLastCommittedBlockIDForNode(
                       *testEngine, NODE_3 ) >= 1 &&
                   ConsensusEngineTestAccess::getCurrentLastCommittedBlockIDForNode(
                       *testEngine, NODE_4 ) >= 1;
        };

        // Stage 5: Verify honest nodes receive the released share and successfully commit Block 1.
        uint32_t storedHeldShares = 0;
        const auto commitDeadline = std::chrono::steady_clock::now() + std::chrono::seconds( 30 );
        while ( std::chrono::steady_clock::now() < commitDeadline ) {
            // The DB may prune Block 1 after later commits, so retain each observation.
            if ( node1Db->haveDecryptionShares( 1, 4 ) ) storedHeldShares |= NODE_1_BIT;
            if ( node3Db->haveDecryptionShares( 1, 1 ) ) storedHeldShares |= NODE_3_BIT;
            if ( node4Db->haveDecryptionShares( 1, 3 ) ) storedHeldShares |= NODE_4_BIT;
            if ( obs.releasedRecipients.load() == ALL_HONEST &&
                 storedHeldShares == ALL_HONEST && honestNodesCommitted() ) {
                break;
            }
            std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
        }

        CATCH_REQUIRE( obs.releasedRecipients.load() == ALL_HONEST );
        CATCH_REQUIRE( storedHeldShares == ALL_HONEST );
        CATCH_REQUIRE( honestNodesCommitted() );

        // Stage 6: Graceful shutdown and cleanup.
        E2EHelper::stopEngineGracefully( testEngine );
    } catch ( ... ) {
        releaseHeldShares.store( true );
        if ( testEngine ) {
            E2EHelper::stopEngineGracefully( testEngine );
        }
        E2EHelper::restoreTestEnvVar( "TEST_TRANSACTIONS_PER_BLOCK", envTransactionCount );
        E2EHelper::restoreTestEnvVar( "TEST_FINALIZATION_DOWNLOAD_ONLY", envDownloadOnly );
        throw;
    }

    E2EHelper::restoreTestEnvVar( "TEST_TRANSACTIONS_PER_BLOCK", envTransactionCount );
    E2EHelper::restoreTestEnvVar( "TEST_FINALIZATION_DOWNLOAD_ONLY", envDownloadOnly );
    CATCH_SUCCEED();
}

}  // namespace ByzantineDecryptionShareTests

#endif  // BITE
