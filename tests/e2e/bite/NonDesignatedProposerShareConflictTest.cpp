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
#include "SkaleCommon.h"
#include "crypto/AESKeyDecryptionShareList.h"
#include "datastructures/BlockProposal.h"
#include "datastructures/CommittedBlock.h"
#include "datastructures/TransactionCiphertextsMap.h"
#include "db/BlockProposalDB.h"
#include "db/ProposalHashDB.h"
#include "db/TEDecryptionDB.h"
#include "node/Node.h"
#include "tests/TestHooks.h"
#include "tests/e2e/ConsensusEngineTestAccess.h"
#include "tests/e2e/E2ETestHelper.h"
#include "tests/harness/transport/SocketIO.h"

#include <atomic>
#include <chrono>
#include <cstdint>

namespace NonDesignatedProposerShareConflictTests {

using namespace std::chrono_literals;
using E2EHelper = E2ETestUtils::E2ETestHelper;
using Chain = E2ETestUtils::FourNodeTestChain;
using E2ETestUtils::WaitResult;
using E2ETestUtils::deadlineIn;
using E2ETestUtils::waitUntil;

// A proposal from a non-designated proposer used to be accepted, and its local decryption
// shares occupied the receiver's decryptor slot, so the decided block could never finalize.
// The receiver now rejects it at admission.
CATCH_TEST_CASE(
    "Receiver rejects non-designated proposal and finalizes the designated block",
    "[bite][byzantine][non-designated-proposer][decryption-share][end-to-end]" ) {

    E2ETestUtils::ScopedTestEnvVar finalizationDownloadOnlyEnv( "TEST_FINALIZATION_DOWNLOAD_ONLY", "1" );
    E2ETestUtils::ScopedTestEnvVar transactionsPerBlockEnv( "TEST_TRANSACTIONS_PER_BLOCK", "8" );
    E2EHelper::configureTestEnvironment( true, Chain::CONFIG_DIR );

    constexpr auto STAGE_TIMEOUT = 30s;
    constexpr auto POLL_INTERVAL = 20ms;

    // Block 1 on four nodes: blockID % nodeCount + 1 == 2 is the designated proposer.
    const block_id BLOCK_ID( 1 );
    const schain_index EXTRA_PROPOSER_INDEX( 1 );
    const schain_index DESIGNATED_PROPOSER_INDEX( 2 );
    const schain_index VICTIM_INDEX( 3 );

    const node_id EXTRA_PROPOSER_NODE = Chain::NODE_1;
    const node_id DESIGNATED_PROPOSER_NODE = Chain::NODE_2;
    const node_id VICTIM_NODE = Chain::NODE_3;
    const node_id OTHER_VALIDATOR_NODE = Chain::NODE_4;

    std::atomic< bool > victimMayAcceptDesignatedProposal{ false };
    std::atomic< bool > othersMayAcceptDesignatedProposal{ false };
    std::atomic< bool > peersMayServeSharesToVictim{ false };
    std::atomic< bool > victimIsHoldingDesignatedProposal{ false };
    std::atomic< uint32_t > designatedSharesServedToVictimCount{ 0 };

    // The victim holds off on accepting the designated proposal, so that it
    // receives the extra one first.
    auto holdDesignatedProposalHook = [&]( node_id, node_id receiverNodeId, block_id blockId,
                                          schain_index proposerIndex, bool& retryLater ) {

        if ( blockId == BLOCK_ID && proposerIndex == DESIGNATED_PROPOSER_INDEX ) {
            if ( receiverNodeId == VICTIM_NODE ) {
                if ( !victimMayAcceptDesignatedProposal.load() ) {
                    victimIsHoldingDesignatedProposal.store( true );
                    retryLater = true;
                }
            } else if ( !othersMayAcceptDesignatedProposal.load() ) {
                retryLater = true;
            }
        }
    };

    auto gateSharesServedToVictimHook = [&]( node_id, node_id requesterNodeId, block_id blockId,
                                            std::shared_ptr< AESKeyDecryptionShareList >& shares,
                                            bool& retryLater ) {

        if ( blockId == BLOCK_ID && requesterNodeId == VICTIM_NODE ) {

            // Peers refuse to serve finalization shares to the victim until released.
            if ( !peersMayServeSharesToVictim.load() ) {
                retryLater = true;

            // After release, count the shares peers serve to the victim for the designated proposal.
            } else if ( shares && shares->getProposerIndex() == DESIGNATED_PROPOSER_INDEX ) {
                designatedSharesServedToVictimCount.fetch_add( 1 );
            }
        }
    };

    // Declared before the hook scopes so hooks are removed first when the test ends early.
    ConsensusEngine* testEngine = nullptr;
    E2ETestUtils::ScopedEngineShutdown engineShutdown( testEngine );

    TestHooks::BlockProposalAdmissionHook::Scope holdDesignatedProposalScope(
        holdDesignatedProposalHook );
    TestHooks::BlockFinalizeResponseHook::Scope gateSharesServedToVictimScope(
        gateSharesServedToVictimHook );
    TestHooks::BlockCatchupResponseHook::Scope noInitialCatchupForVictimScope(
        E2ETestUtils::suppressInitialCatchup( VICTIM_NODE ) );

    E2EHelper::setupEngine( testEngine, 0 );
    for ( auto nodeId :
        { EXTRA_PROPOSER_NODE, DESIGNATED_PROPOSER_NODE, VICTIM_NODE, OTHER_VALIDATOR_NODE } ) {
        CATCH_INFO( "SGX test keys are required for node " << nodeId );
        CATCH_REQUIRE( ConsensusEngineTestAccess::getNode( *testEngine, nodeId )->isSgxEnabled() );
    }
    testEngine->slowStartBootStrapTest();

    auto extraProposer = ConsensusEngineTestAccess::getNode( *testEngine, EXTRA_PROPOSER_NODE );
    auto victim = ConsensusEngineTestAccess::getNode( *testEngine, VICTIM_NODE );
    auto victimProposalDb = victim->getBlockProposalDB();
    auto victimDecryptionDb = victim->getTEDecryptionDB();
    CATCH_REQUIRE( victimDecryptionDb );

    // The victim's own decryptor slot in its in-memory share store.
    auto readVictimOwnShareSlot = [&] {
        return ConsensusEngineTestAccess::getStoredDecryptionShareList(
            *testEngine, VICTIM_NODE, BLOCK_ID, VICTIM_INDEX );
    };

    auto becomesTrueWithinStageTimeout = [&]( auto predicate ) {
        return waitUntil( predicate, deadlineIn( STAGE_TIMEOUT ), POLL_INTERVAL ) ==
               WaitResult::Satisfied;
    };

    // Stage 1: both non-designated proposals exist, and the designated proposal is held.
    ptr< BlockProposal > extraProposal;
    ptr< BlockProposal > victimOwnProposal;
    CATCH_REQUIRE( becomesTrueWithinStageTimeout( [&] {
        extraProposal = extraProposer->getBlockProposalDB()->getBlockProposal(
            BLOCK_ID, EXTRA_PROPOSER_INDEX );
        victimOwnProposal = victimProposalDb->getBlockProposal( BLOCK_ID, VICTIM_INDEX );
        return extraProposal && victimOwnProposal && victimIsHoldingDesignatedProposal.load();
    } ) );

    // The victim's own non-designated proposal never scheduled local shares.
    CATCH_REQUIRE( victimOwnProposal->getMyDecryptionSharesState() ==
                   BlockProposal::MyDecryptionSharesState::NotStarted );
    CATCH_REQUIRE( victimDecryptionDb->getDecryptionsCount( BLOCK_ID ) == 0 );
    CATCH_REQUIRE( victimProposalDb->getBlockProposal( BLOCK_ID, DESIGNATED_PROPOSER_INDEX ) ==
                   nullptr );

    CATCH_REQUIRE_FALSE( extraProposal->getSignature().empty() );
    CATCH_REQUIRE( extraProposal->getTransactionCiphertexts() );
    CATCH_REQUIRE( extraProposal->getTransactionCiphertexts()->totalCiphertextCount() > 0 );

    // Stage 2: the extra proposal goes through the real exchange and is rejected at admission.
    // sendProposalToPeer() returns once the victim answers with anything but CONNECTION_RETRY_LATER.
    ConsensusEngineTestAccess::sendProposalToPeer(
        *testEngine, EXTRA_PROPOSER_NODE, extraProposal, VICTIM_INDEX );

    // Admission rejects before the proposal hash is recorded and before local shares are scheduled.
    CATCH_REQUIRE_FALSE( victim->getProposalHashDB()->haveProposal( BLOCK_ID, EXTRA_PROPOSER_INDEX ) );
    CATCH_REQUIRE( victimProposalDb->getBlockProposal( BLOCK_ID, EXTRA_PROPOSER_INDEX ) == nullptr );
    CATCH_REQUIRE( readVictimOwnShareSlot() == nullptr );
    CATCH_REQUIRE( victimDecryptionDb->getDecryptionsCount( BLOCK_ID ) == 0 );

    // Stage 3: the designated proposal's local shares fill the victim's own slot.
    victimMayAcceptDesignatedProposal.store( true );

    // getMyDecryptionShares() is non-null only once local shares are Ready.
    ptr< BlockProposal > designatedProposalAtVictim;
    CATCH_REQUIRE( becomesTrueWithinStageTimeout( [&] {
        designatedProposalAtVictim =
            victimProposalDb->getBlockProposal( BLOCK_ID, DESIGNATED_PROPOSER_INDEX );
        return designatedProposalAtVictim && designatedProposalAtVictim->getMyDecryptionShares();
    } ) );

    auto victimSharesForDesignatedProposal = designatedProposalAtVictim->getMyDecryptionShares();
    CATCH_REQUIRE( victimSharesForDesignatedProposal );
    CATCH_REQUIRE( victimSharesForDesignatedProposal->getProposerIndex() == DESIGNATED_PROPOSER_INDEX );
    CATCH_REQUIRE( victimSharesForDesignatedProposal->getDecryptorIndex() == VICTIM_INDEX );
    CATCH_REQUIRE( victimSharesForDesignatedProposal->totalCiphertextSharesCount() > 0 );

    auto victimOwnShareSlot = readVictimOwnShareSlot();
    CATCH_REQUIRE( victimOwnShareSlot );
    CATCH_REQUIRE( victimOwnShareSlot->getBlockId() == BLOCK_ID );
    CATCH_REQUIRE( victimOwnShareSlot->getProposerIndex() == DESIGNATED_PROPOSER_INDEX );
    CATCH_REQUIRE( victimOwnShareSlot->getDecryptorIndex() == VICTIM_INDEX );
    CATCH_REQUIRE( victimDecryptionDb->getDecryptionsCount( BLOCK_ID ) == 1 );

    // Stage 4: consensus decides the designated proposal, peers serve their shares,
    // and every node, including the victim, finalizes the block.
    peersMayServeSharesToVictim.store( true );
    othersMayAcceptDesignatedProposal.store( true );

    CATCH_REQUIRE( becomesTrueWithinStageTimeout( [&] {
        return ConsensusEngineTestAccess::haveAllCommitted( *testEngine,
                   { EXTRA_PROPOSER_NODE, DESIGNATED_PROPOSER_NODE, VICTIM_NODE,
                       OTHER_VALIDATOR_NODE },
                   BLOCK_ID ) &&
               designatedSharesServedToVictimCount.load() > 0;
    } ) );

    auto committedBlockAtVictim = ConsensusEngineTestAccess::getCommittedBlockForBlockIdForNode(
        *testEngine, static_cast< uint64_t >( BLOCK_ID ), VICTIM_NODE );
    CATCH_REQUIRE( committedBlockAtVictim );
    CATCH_REQUIRE( committedBlockAtVictim->getProposerIndex() == DESIGNATED_PROPOSER_INDEX );

    E2EHelper::stopEngineGracefully( testEngine );
}

}  // namespace NonDesignatedProposerShareConflictTests

#endif  // BITE
