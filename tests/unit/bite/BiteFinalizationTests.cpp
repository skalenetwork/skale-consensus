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

#ifdef BITE

#include <chrono>
#include <thread>

#include "BiteTestUtils.h"
#include "bite/BiteManager.h"
#include "crypto/MockupSignature.h"
#include "datastructures/BlockProposal.h"
#include "datastructures/DAProof.h"
#include "db/DAProofDB.h"
#include "db/TEDecryptionDB.h"
#include "node/NodeInfo.h"
#include "tests/TestUtils.h"

using namespace std;
using namespace BiteTestUtils;

namespace {

// Build a local 4-node fixture: one Node with 4 NodeInfos registered (indices 1-4),
// giving totalSigners=4 and requiredSigners=3 at TEDecryptionDB construction time.
// The local node is index 1.
void make4NodeFixture(
    ConsensusEngine& engine,
    shared_ptr<Schain>& chain_out,
    shared_ptr<Node>& node_out
) {
    nlohmann::json cfg;
    cfg["nodeID"] = 1;
    cfg["nodeName"] = "testNode4";
    cfg["bindIP"] = "127.0.0.1";
    cfg["basePort"] = 10200;

    node_out = TestUtils::createTestNode(cfg, &engine);

    const schain_id schainId( 1337 );
    // Index 1 matches node->getNodeID() so the Schain constructor finds thisNodeInfo.
    node_out->setNodeInfo(
        make_shared<NodeInfo>( node_id( 1 ), "127.0.0.1", 10200, schainId, schain_index( 1 ) ) );
    node_out->setNodeInfo(
        make_shared<NodeInfo>( node_id( 2 ), "127.0.0.1", 10210, schainId, schain_index( 2 ) ) );
    node_out->setNodeInfo(
        make_shared<NodeInfo>( node_id( 3 ), "127.0.0.1", 10220, schainId, schain_index( 3 ) ) );
    node_out->setNodeInfo(
        make_shared<NodeInfo>( node_id( 4 ), "127.0.0.1", 10230, schainId, schain_index( 4 ) ) );

    string schainName = "testChain4";
    chain_out = TestUtils::createTestSchain( node_out, schain_index( 1 ), schainId, schainName );
    node_out->setSchain( chain_out );
    ConsensusEngineTestAccess::registerNode( engine, node_out );
}

}  // namespace

// A node whose local SGX share failed must still finalize
// a decided block when a full threshold of valid foreign shares is available.
//
// This test fails on current code at the CHECK_STATE2(myDecryptionShares, ...) guard in
// Schain::finalizeDecidedAndSignedBlockInThread. It passes after the minimal fix that
// removes that hard requirement and allows the node to proceed with foreign shares.
CATCH_TEST_CASE(
    "Finalization succeeds using foreign threshold shares when local SGX share failed",
    "[bite][finalization][foreign-shares][regression]" ) {

    // ---- 4-node / threshold-3 fixture ----
    ConsensusEngine engine( 0, 100000000 );
    TempDbDir tempDb( engine );  // isolated per-run LevelDB dir; cleaned up on scope exit
    shared_ptr<Schain> chain;
    shared_ptr<Node> node;
    make4NodeFixture( engine, chain, node );

    auto cryptoManager = make_shared<CryptoManager>( *chain );

    // bootstrap() sets isStateInitialized = true, which blockCommitArrived requires.
    // It also calls proposeNextBlock() which fires an async task for block_id(1), but
    // since the bootstrap proposal has no BITE transactions, that task returns early
    // without writing to TEDecryptionDB (see BiteManager::computeOrLoadMyDecryptionShares).
    chain->bootstrap( 0, MODERN_TIME + 1, 0 );

    // ---- build proposal and populate its ciphertext map ----
    // Use block_id(1) — blockCommitArrived and processCommittedBlock both require
    // getLastCommittedBlockID() + 1 == blockId, and lastCommitted starts at 0
    // after a fresh bootstrap, so only block_id(1) is valid.
    auto kp = generateKeys( 1, 1 );
    auto proposal = makeAsyncTestProposal( chain, cryptoManager, block_id( 1 ), kp );

    auto biteManager = chain->getBiteManager();
    CATCH_REQUIRE( biteManager );

    // In mockup mode computeAndValidateSGXAESKeyBatch is a no-op (returns immediately
    // when !usingRealCrypto). getTransactionCiphertexts() is populated inside
    // createMyProposal -> parseBITETransactions. The call mirrors the production
    // stage ordering and confirms no parse/validation failures occurred.
    biteManager->computeAndValidateSGXAESKeyBatch( proposal );
    CATCH_REQUIRE( proposal->getFailedTransactionsRef().empty() );

    // ---- simulate local SGX failure ----
    // Local share generation is an infrastructure operation; failure here must
    // never produce a failed-transaction entry.
    CATCH_REQUIRE( proposal->tryBeginMyDecryptionSharesComputation() );
    proposal->markMyDecryptionSharesFailed();
    CATCH_REQUIRE( proposal->getFailedTransactionsRef().empty() );

    // ---- supply three foreign mock share lists (decryptors 2, 3, 4) ----
    // addDecryptionShares blocks until all Folly validation futures complete,
    // so there is no race between share population and the finalization executor.
    // Using indices 2-4 (not 1) because bootstrap fires an async task that calls
    // computeOrLoadMyDecryptionShares for its block_id(1) proposal. With the fix in
    // place that task returns early (no addMyDecryptionShares) for proposals with
    // 0 BITE ciphertexts, so it no longer occupies decryptorIndex=1 in the DB.
    auto teDB = node->getTEDecryptionDB();
    for ( int idx = 2; idx <= 4; ++idx ) {
        auto shares = makeMockupShareList( proposal, schain_index( idx ) );
        teDB->addDecryptionShares( shares );
    }

    // ---- register proposal in BlockProposalDB ----
    chain->proposedBlockArrived( proposal );

    // ---- add one mock DA proof ----
    // In mockup mode verifyDAProofThresholdSig checks: sig.toString() == hash.toHex().
    // Supplying the hash hex directly as the signature string satisfies this check.
    auto hashHex = proposal->getHash().toHex();
    ptr<ThresholdSignature> mockDASig =
        make_shared<MockupSignature>( hashHex, proposal->getBlockID(), 4, 3 );
    auto daProof = make_shared<DAProof>( proposal, mockDASig );
    node->getDaProofDB()->addDAProof( daProof );

    // Fast-fail: if share setup is broken, fail here with a clear count mismatch
    // rather than timing out silently at the commit poll below.
    CATCH_REQUIRE( teDB->getDecryptionsCount( proposal->getBlockID() ) == 3 );

    // ---- trigger finalization via the public entry point ----
    // nullptr for _reencryptionThresholdSig: BITE2 patch is inactive at MODERN_TIME+1
    // (bite2PatchTimestamp defaults to 0), so blockCommitArrived asserts it is null.
    ptr<ThresholdSignature> consensusSig =
        make_shared<MockupSignature>( hashHex, proposal->getBlockID(), 4, 3 );
    chain->finalizeDecidedAndSignedBlock(
        proposal->getBlockID(),
        proposal->getProposerIndex(),
        consensusSig,
        nullptr );

    // ---- wait for commit (bounded poll, 5 s) ----
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( 5 );
    while ( static_cast<uint64_t>( chain->getLastCommittedBlockID() ) <
                static_cast<uint64_t>( proposal->getBlockID() ) &&
            std::chrono::steady_clock::now() < deadline ) {
        std::this_thread::sleep_for( std::chrono::milliseconds( 20 ) );
    }

    CATCH_REQUIRE( static_cast<uint64_t>( chain->getLastCommittedBlockID() ) ==
                   static_cast<uint64_t>( proposal->getBlockID() ) );
    // Infrastructure (SGX) failure must never create a parse/validation failure entry
    // on the proposal. getFailedTransactionsRef() records only stage-1/2 failures,
    // not AES decryption failures that happen later inside the finalization executor.
    CATCH_REQUIRE( proposal->getFailedTransactionsRef().empty() );
}

// A node whose local SGX share computation is still in-progress (InProgress state,
// i.e. the async task was launched but has not resolved yet) must still finalize
// a decided block when a full threshold of valid foreign shares is available.
//
// This exercises the same null-check branch as the Failed test but confirms that
// InProgress also results in getMyDecryptionShares() == nullptr, so finalization
// does not stall waiting for the local task to complete.
CATCH_TEST_CASE(
    "Finalization succeeds using foreign threshold shares when local SGX share is still in-progress",
    "[bite][finalization][foreign-shares][regression]" ) {

    ConsensusEngine engine( 0, 100000000 );
    TempDbDir tempDb( engine );
    shared_ptr<Schain> chain;
    shared_ptr<Node> node;
    make4NodeFixture( engine, chain, node );

    auto cryptoManager = make_shared<CryptoManager>( *chain );
    chain->bootstrap( 0, MODERN_TIME + 1, 0 );

    auto kp = generateKeys( 1, 1 );
    auto proposal = makeAsyncTestProposal( chain, cryptoManager, block_id( 1 ), kp );

    auto biteManager = chain->getBiteManager();
    CATCH_REQUIRE( biteManager );
    biteManager->computeAndValidateSGXAESKeyBatch( proposal );
    CATCH_REQUIRE( proposal->getFailedTransactionsRef().empty() );

    // Simulate the async SGX task having been launched but not yet resolved.
    // tryBeginMyDecryptionSharesComputation transitions state to InProgress.
    // Neither markMyDecryptionSharesReady nor markMyDecryptionSharesFailed is
    // called, leaving the proposal permanently in InProgress.
    // getMyDecryptionShares() returns nullptr in this state, so finalization
    // must proceed on foreign shares alone without waiting for resolution.
    CATCH_REQUIRE( proposal->tryBeginMyDecryptionSharesComputation() );
    CATCH_REQUIRE( proposal->getMyDecryptionShares() == nullptr );

    auto teDB = node->getTEDecryptionDB();
    for ( int idx = 2; idx <= 4; ++idx ) {
        auto shares = makeMockupShareList( proposal, schain_index( idx ) );
        teDB->addDecryptionShares( shares );
    }

    chain->proposedBlockArrived( proposal );

    auto hashHex = proposal->getHash().toHex();
    ptr<ThresholdSignature> mockDASig =
        make_shared<MockupSignature>( hashHex, proposal->getBlockID(), 4, 3 );
    auto daProof = make_shared<DAProof>( proposal, mockDASig );
    node->getDaProofDB()->addDAProof( daProof );

    CATCH_REQUIRE( teDB->getDecryptionsCount( proposal->getBlockID() ) == 3 );

    ptr<ThresholdSignature> consensusSig =
        make_shared<MockupSignature>( hashHex, proposal->getBlockID(), 4, 3 );
    chain->finalizeDecidedAndSignedBlock(
        proposal->getBlockID(),
        proposal->getProposerIndex(),
        consensusSig,
        nullptr );

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( 5 );
    while ( static_cast<uint64_t>( chain->getLastCommittedBlockID() ) <
                static_cast<uint64_t>( proposal->getBlockID() ) &&
            std::chrono::steady_clock::now() < deadline ) {
        std::this_thread::sleep_for( std::chrono::milliseconds( 20 ) );
    }

    CATCH_REQUIRE( static_cast<uint64_t>( chain->getLastCommittedBlockID() ) ==
                   static_cast<uint64_t>( proposal->getBlockID() ) );
    CATCH_REQUIRE( proposal->getFailedTransactionsRef().empty() );
}

#endif
