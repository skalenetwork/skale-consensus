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

// Holds the Node/Schain pair produced by make4NodeFixture().
struct NodeFixture {
    shared_ptr<Schain> chain;
    shared_ptr<Node> node;
};

// Build a local 4-node fixture: one Node with 4 NodeInfos registered (indices 1-4),
// giving totalSigners=4 and requiredSigners=3 at TEDecryptionDB construction time.
// The local node is index 1.
NodeFixture make4NodeFixture( ConsensusEngine& engine ) {
    nlohmann::json cfg;
    cfg["nodeID"] = 1;
    cfg["nodeName"] = "testNode4";
    cfg["bindIP"] = "127.0.0.1";
    cfg["basePort"] = 10200;

    auto node = TestUtils::createTestNode( cfg, &engine );

    const schain_id schainId( 1337 );
    // Index 1 matches node->getNodeID() so the Schain constructor finds thisNodeInfo.
    node->setNodeInfo(
        make_shared<NodeInfo>( node_id( 1 ), "127.0.0.1", 10200, schainId, schain_index( 1 ) ) );
    node->setNodeInfo(
        make_shared<NodeInfo>( node_id( 2 ), "127.0.0.1", 10210, schainId, schain_index( 2 ) ) );
    node->setNodeInfo(
        make_shared<NodeInfo>( node_id( 3 ), "127.0.0.1", 10220, schainId, schain_index( 3 ) ) );
    node->setNodeInfo(
        make_shared<NodeInfo>( node_id( 4 ), "127.0.0.1", 10230, schainId, schain_index( 4 ) ) );

    string schainName = "testChain4";
    auto chain = TestUtils::createTestSchain( node, schain_index( 1 ), schainId, schainName );
    node->setSchain( chain );
    ConsensusEngineTestAccess::registerNode( engine, node );

    return { chain, node };
}

// Polls until chain has committed up to _blockId, or _timeout elapses.
void waitForCommit( const shared_ptr<Schain>& chain, block_id _blockId,
    std::chrono::milliseconds _timeout = std::chrono::seconds( 5 ) ) {
    const auto deadline = std::chrono::steady_clock::now() + _timeout;
    while ( static_cast<uint64_t>( chain->getLastCommittedBlockID() ) <
                static_cast<uint64_t>( _blockId ) &&
            std::chrono::steady_clock::now() < deadline ) {
        std::this_thread::sleep_for( std::chrono::milliseconds( 20 ) );
    }
}

}  // namespace

// A failed local SGX share must not block finalization once a foreign threshold is met.
CATCH_TEST_CASE(
    "Finalization succeeds using foreign threshold shares when local SGX share failed",
    "[bite][finalization][foreign-shares][regression]" ) {

    auto kp = generateKeys( 1, 1 );
    ConsensusEngine engine( 0, 100000000 );
    TempDbDir tempDb( engine );  // isolated per-run LevelDB dir
    auto [chain, node] = make4NodeFixture( engine );

    auto cryptoManager = make_shared<CryptoManager>( *chain );
    chain->bootstrap( 0, MODERN_TIME + 1, 0 );

    auto proposal = makeTestProposal( chain, cryptoManager, block_id( 1 ), kp );
    auto biteManager = chain->getBiteManager();
    CATCH_REQUIRE( biteManager );
    biteManager->computeAndValidateSGXAESKeyBatch( proposal );
    CATCH_REQUIRE( proposal->getFailedTransactionsRef().empty() );

    // Simulate local SGX share failure — must not register as a failed transaction.
    CATCH_REQUIRE( proposal->tryBeginMyDecryptionSharesComputation() );
    proposal->markMyDecryptionSharesFailed();
    CATCH_REQUIRE( proposal->getFailedTransactionsRef().empty() );

    // Foreign shares from decryptors 2-4 (index 1 is reserved for the local node).
    auto teDB = node->getTEDecryptionDB();
    for ( int idx = 2; idx <= 4; ++idx ) {
        auto shares = makeMockupShareList( proposal, schain_index( idx ) );
        teDB->addDecryptionShares( shares );
    }
    CATCH_REQUIRE( teDB->getDecryptionsCount( proposal->getBlockID() ) == 3 );

    // register proposal in block proposal DB
    chain->proposedBlockArrived( proposal );

    // Mock DA proof: mockup mode accepts sig.toString() == hash.toHex().
    auto hashHex = proposal->getHash().toHex();
    ptr<ThresholdSignature> mockDASig =
        make_shared<MockupSignature>( hashHex, proposal->getBlockID(), 4, 3 );
    auto daProof = make_shared<DAProof>( proposal, mockDASig );
    node->getDaProofDB()->addDAProof( daProof );

    // nullptr reencryption sig: BITE2 patch inactive at MODERN_TIME+1.
    ptr<ThresholdSignature> consensusSig =
        make_shared<MockupSignature>( hashHex, proposal->getBlockID(), 4, 3 );
    chain->finalizeDecidedAndSignedBlock(
        proposal->getBlockID(),
        proposal->getProposerIndex(),
        consensusSig,
        nullptr );

    waitForCommit( chain, proposal->getBlockID() );

    CATCH_REQUIRE( static_cast<uint64_t>( chain->getLastCommittedBlockID() ) ==
                   static_cast<uint64_t>( proposal->getBlockID() ) );
    // SGX (infrastructure) failure must never create a failed-transaction entry.
    CATCH_REQUIRE( proposal->getFailedTransactionsRef().empty() );
}

// Same as above, but the local SGX task is left in InProgress (never resolved)
// instead of Failed — finalization must not stall waiting for it.
CATCH_TEST_CASE(
    "Finalization succeeds using foreign threshold shares when local SGX share is still in-progress",
    "[bite][finalization][foreign-shares][regression]" ) {

    ConsensusEngine engine( 0, 100000000 );
    TempDbDir tempDb( engine );
    auto [chain, node] = make4NodeFixture( engine );

    auto cryptoManager = make_shared<CryptoManager>( *chain );
    chain->bootstrap( 0, MODERN_TIME + 1, 0 );

    auto kp = generateKeys( 1, 1 );
    auto proposal = makeTestProposal( chain, cryptoManager, block_id( 1 ), kp );

    auto biteManager = chain->getBiteManager();
    CATCH_REQUIRE( biteManager );
    biteManager->computeAndValidateSGXAESKeyBatch( proposal );
    CATCH_REQUIRE( proposal->getFailedTransactionsRef().empty() );

    // Leave the local share computation InProgress: getMyDecryptionShares() == nullptr.
    CATCH_REQUIRE( proposal->tryBeginMyDecryptionSharesComputation() );
    CATCH_REQUIRE( proposal->getMyDecryptionShares() == nullptr );

    auto teDB = node->getTEDecryptionDB();
    for ( int idx = 2; idx <= 4; ++idx ) {
        auto shares = makeMockupShareList( proposal, schain_index( idx ) );
        teDB->addDecryptionShares( shares );
    }
    CATCH_REQUIRE( teDB->getDecryptionsCount( proposal->getBlockID() ) == 3 );

    chain->proposedBlockArrived( proposal );

    auto hashHex = proposal->getHash().toHex();
    ptr<ThresholdSignature> mockDASig =
        make_shared<MockupSignature>( hashHex, proposal->getBlockID(), 4, 3 );
    auto daProof = make_shared<DAProof>( proposal, mockDASig );
    node->getDaProofDB()->addDAProof( daProof );

    ptr<ThresholdSignature> consensusSig =
        make_shared<MockupSignature>( hashHex, proposal->getBlockID(), 4, 3 );
    chain->finalizeDecidedAndSignedBlock(
        proposal->getBlockID(),
        proposal->getProposerIndex(),
        consensusSig,
        nullptr );

    waitForCommit( chain, proposal->getBlockID() );

    CATCH_REQUIRE( static_cast<uint64_t>( chain->getLastCommittedBlockID() ) ==
                   static_cast<uint64_t>( proposal->getBlockID() ) );
    CATCH_REQUIRE( proposal->getFailedTransactionsRef().empty() );
}

// node only needs a threshold including its own. Even for blocks with no ciphertexts,
// local node stores an empty decryption shares list that is accounted for the threshold.
CATCH_TEST_CASE(
    "Finalization succeeds for an empty-ciphertext block when one of four nodes is down",
    "[bite][finalization][foreign-shares][regression]" ) {

    ConsensusEngine engine( 0, 100000000 );
    TempDbDir tempDb( engine );
    auto [chain, node] = make4NodeFixture( engine );

    auto cryptoManager = make_shared<CryptoManager>( *chain );
    chain->bootstrap( 0, MODERN_TIME + 1, 0 );

    // This node's own honest proposal, with no BITE transactions at all.
    auto proposal = makeEmptyTestProposal( chain, cryptoManager, block_id( 1 ) );
    auto biteManager = chain->getBiteManager();
    CATCH_REQUIRE( biteManager );
    biteManager->computeAndValidateSGXAESKeyBatch( proposal );
    CATCH_REQUIRE( proposal->getFailedTransactionsRef().empty() );

    // Local SGX share computation succeeds and is stored right away - this is
    // exactly the behavior restored by removing BiteManager's early-return for
    // zero-ciphertext proposals.
    biteManager->ensureMyDecryptionSharesAreComputed( proposal );
    CATCH_REQUIRE( proposal->getMyDecryptionShares() );

    // Only 2 of the remaining 3 nodes (indices 2,3) respond - index 4 is down.
    auto teDB = node->getTEDecryptionDB();
    for ( int idx = 2; idx <= 3; ++idx ) {
        auto shares = makeMockupShareList( proposal, schain_index( idx ) );
        teDB->addDecryptionShares( shares );
    }
    // local (1) + foreign (2, 3) == requiredSigners (3).
    CATCH_REQUIRE( teDB->getDecryptionsCount( proposal->getBlockID() ) == 3 );

    chain->proposedBlockArrived( proposal );

    auto hashHex = proposal->getHash().toHex();
    ptr<ThresholdSignature> mockDASig =
        make_shared<MockupSignature>( hashHex, proposal->getBlockID(), 4, 3 );
    auto daProof = make_shared<DAProof>( proposal, mockDASig );
    node->getDaProofDB()->addDAProof( daProof );

    ptr<ThresholdSignature> consensusSig =
        make_shared<MockupSignature>( hashHex, proposal->getBlockID(), 4, 3 );
    chain->finalizeDecidedAndSignedBlock(
        proposal->getBlockID(),
        proposal->getProposerIndex(),
        consensusSig,
        nullptr );

    waitForCommit( chain, proposal->getBlockID() );

    CATCH_REQUIRE( static_cast<uint64_t>( chain->getLastCommittedBlockID() ) ==
                   static_cast<uint64_t>( proposal->getBlockID() ) );
    CATCH_REQUIRE( proposal->getFailedTransactionsRef().empty() );
}

#endif
