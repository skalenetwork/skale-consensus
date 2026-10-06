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
#include <future>
#include <thread>

#include "BiteTestUtils.h"
#include "bite/BiteManager.h"
#include "blockfinalize/client/BlockFinalizeDownloader.h"
#include "crypto/DecryptedAESKeyList.h"
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


CATCH_TEST_CASE(
    "Finalization downloader keeps collecting after the raw share threshold",
    "[bite][finalization][share-progress][regression]" ) {
    ConsensusEngine engine( 0, 100000000 );
    TempDbDir tempDb( engine );
    auto [chain, node] = make4NodeFixture( engine );
    auto cryptoManager = make_shared<CryptoManager>( *chain );
    chain->bootstrap( 0, MODERN_TIME + 1, 0 );
    auto proposal = makeEmptyTestProposal( chain, cryptoManager, block_id( 1 ) );
    chain->getBiteManager()->computeAndValidateSGXAESKeyBatch( proposal );
    chain->proposedBlockArrived( proposal );
    ptr<ThresholdSignature> daSig =
        make_shared<MockupSignature>( proposal->getHash().toHex(), block_id( 1 ), 4, 3 );
    node->getDaProofDB()->addDAProof( make_shared<DAProof>( proposal, daSig ) );

    auto teDB = node->getTEDecryptionDB();
    for ( int idx = 1; idx <= 3; ++idx ) {
        teDB->addDecryptionShares( makeMockupShareList( proposal, schain_index( idx ) ) );
    }
    CATCH_REQUIRE( chain->haveAllElementsToFinalizeBlock( block_id( 1 ),
        proposal->getProposerIndex() ) );

    BlockFinalizeDownloader downloader( chain.get(), block_id( 1 ),
        node->getCurrentEpochId(), proposal->getProposerIndex() );
    CATCH_REQUIRE( downloader.needDecryptionShares( schain_index( 4 ) ) );
    // A failed request retains its positive fragment index and must keep retrying.
    CATCH_REQUIRE_FALSE( downloader.exitDownloadLoop( 1 ) );
    // A worker that delivered its peer's shares can exit independently.
    CATCH_REQUIRE( downloader.exitDownloadLoop( 0 ) );
    CATCH_REQUIRE_FALSE( downloader.exitDownloadLoop( 1 ) );
    downloader.requestStop();
    CATCH_REQUIRE( downloader.exitDownloadLoop( 1 ) );
}

CATCH_TEST_CASE(
    "Finalization retries a failed share snapshot when a share arrives before waiting",
    "[bite][finalization][share-progress][regression]" ) {
    ConsensusEngine engine( 0, 100000000 );
    TempDbDir tempDb( engine );
    auto [chain, node] = make4NodeFixture( engine );
    auto cryptoManager = make_shared<CryptoManager>( *chain );
    chain->bootstrap( 0, MODERN_TIME + 1, 0 );
    auto kp = generateKeys( 1, 1 );
    auto proposal = makeTestProposal( chain, cryptoManager, block_id( 1 ), kp );
    chain->getBiteManager()->computeAndValidateSGXAESKeyBatch( proposal );
    auto teDB = node->getTEDecryptionDB();
    teDB->addDecryptionShares( makeMockupShareList( proposal, schain_index( 2 ) ) );
    teDB->addDecryptionShares( makeMockupShareList( proposal, schain_index( 3 ) ) );

    uint64_t sharesUsed = 0;
    CATCH_REQUIRE_THROWS( teDB->mergeAESKeys( block_id( 1 ),
        proposal->getTransactionCiphertexts(), &sharesUsed ) );
    CATCH_REQUIRE( sharesUsed == 2 );

    // Progress happened before the caller entered its wait: it must not miss it.
    teDB->addMyDecryptionShares( makeMockupShareList( proposal, schain_index( 1 ) ) );
    CATCH_REQUIRE( teDB->waitForMoreDecryptionShares(
        block_id( 1 ), sharesUsed, std::chrono::milliseconds( 0 ) ) );
    CATCH_REQUIRE( teDB->mergeAESKeys( block_id( 1 ),
        proposal->getTransactionCiphertexts(), &sharesUsed ) );
    CATCH_REQUIRE( sharesUsed == 3 );
    CATCH_REQUIRE_FALSE( teDB->haveDecryptionShares( block_id( 1 ), schain_index( 4 ) ) );

    // A duplicate must not count as progress or cause repeated merges of the same set.
    teDB->addDecryptionShares( makeMockupShareList( proposal, schain_index( 3 ) ) );
    CATCH_REQUIRE_FALSE( teDB->waitForMoreDecryptionShares(
        block_id( 1 ), sharesUsed, std::chrono::milliseconds( 0 ) ) );
}

CATCH_TEST_CASE(
    "Finalization share wait observes a delayed local share after peers finish",
    "[bite][finalization][share-progress][regression]" ) {
    ConsensusEngine engine( 0, 100000000 );
    TempDbDir tempDb( engine );
    auto [chain, node] = make4NodeFixture( engine );
    auto cryptoManager = make_shared<CryptoManager>( *chain );
    chain->bootstrap( 0, MODERN_TIME + 1, 0 );
    auto proposal = makeEmptyTestProposal( chain, cryptoManager, block_id( 1 ) );
    chain->getBiteManager()->computeAndValidateSGXAESKeyBatch( proposal );
    auto teDB = node->getTEDecryptionDB();
    for ( int idx = 2; idx <= 4; ++idx ) {
        teDB->addDecryptionShares( makeMockupShareList( proposal, schain_index( idx ) ) );
    }

    // No peer worker will supply another list; only local SGX can make progress.
    auto waiter = std::async( std::launch::async, [&] {
        return teDB->waitForMoreDecryptionShares(
            block_id( 1 ), 3, std::chrono::seconds( 2 ) );
    } );
    CATCH_REQUIRE( waiter.wait_for( std::chrono::milliseconds( 20 ) ) ==
                   std::future_status::timeout );
    teDB->addMyDecryptionShares( makeMockupShareList( proposal, schain_index( 1 ) ) );
    CATCH_REQUIRE( waiter.wait_for( std::chrono::seconds( 1 ) ) == std::future_status::ready );
    CATCH_REQUIRE( waiter.get() );
    CATCH_REQUIRE( teDB->getDecryptionsCount( block_id( 1 ) ) == 4 );

    // No further arrivals: the default bounded wait returns control for exit/catchup checks.
    auto idleWaiter = std::async( std::launch::async, [&] {
        return teDB->waitForMoreDecryptionShares( block_id( 1 ), 4 );
    } );
    CATCH_REQUIRE( idleWaiter.wait_for( std::chrono::seconds( 1 ) ) == std::future_status::ready );
    CATCH_REQUIRE_FALSE( idleWaiter.get() );
}

// A Byzantine peer whose share list arrives first, and does not match the proposal, must only
// ever occupy its own slot. The first stored list must not become the "expected shape" that
// rejects honest lists, and the merge must skip the bad list instead of failing on it.
// Stages fail in order, so the failing assertion shows which part of the fix is still missing:
//   1. honest peer lists are rejected at ingestion (TEDecryptionDB::addDecryptionShares)
//   2. our own shares are rejected, and never persisted (TEDecryptionDB::addMyDecryptionShares)
//   3. the merge trusts the lowest-index list as reference (BiteEngine::mergeAESKeys)
CATCH_TEST_CASE(
    "Byzantine share list of unexpected shape arriving first must not block honest share lists",
    "[bite][finalization][byzantine-share][regression]" ) {
    ConsensusEngine engine( 0, 100000000 );
    TempDbDir tempDb( engine );
    auto [chain, node] = make4NodeFixture( engine );
    auto cryptoManager = make_shared<CryptoManager>( *chain );
    chain->bootstrap( 0, MODERN_TIME + 1, 0 );
    auto kp = generateKeys( 1, 1 );
    // bootstrap() proposes block 1 for this node and may store its own shares for it.
    // Use block 2 so that this test owns every decryptor slot.
    const block_id blockId( 2 );
    auto proposal = makeTestProposal( chain, cryptoManager, blockId, kp );
    chain->getBiteManager()->computeAndValidateSGXAESKeyBatch( proposal );
    // The honest lists must have a non-empty shape for the Byzantine list to be a mismatch.
    const auto expectedCiphertextCount = proposal->getTransactionCiphertexts()->totalCiphertextCount();
    CATCH_REQUIRE( expectedCiphertextCount > 0 );

    auto teDB = node->getTEDecryptionDB();
    const auto proposerIndex = proposal->getProposerIndex();
    CATCH_REQUIRE( teDB->getDecryptionsCount( blockId ) == 0 );

    // Decryptor 2 is Byzantine and its list is the first one to reach the DB.
    ptr<AESKeyDecryptionShareList> byzantineList;

    CATCH_SECTION( "empty list" ) {
        byzantineList = make_shared<AESKeyDecryptionShareList>(
            blockId, proposerIndex, schain_index( 2 ) );
        CATCH_REQUIRE( byzantineList->totalCiphertextSharesCount() == 0 );
    }

    // Same total share count as an honest list, so a count comparison cannot detect it.
    CATCH_SECTION( "same share count, but for a transaction that is not in the proposal" ) {
        auto honestLike = makeMockupShareList( proposal, schain_index( 2 ) );
        const auto firstTxIdx = proposal->getTransactionCiphertexts()->begin()->first;
        byzantineList = make_shared<AESKeyDecryptionShareList>(
            blockId, proposerIndex, schain_index( 2 ) );
        byzantineList->addShares(
            transaction_index( 99 ), honestLike->getDecryptionShares( firstTxIdx ) );
        CATCH_REQUIRE( byzantineList->totalCiphertextSharesCount() == expectedCiphertextCount );
    }

    CATCH_REQUIRE( byzantineList );
    CATCH_REQUIRE_NOTHROW( teDB->addDecryptionShares( byzantineList ) );
    CATCH_REQUIRE( teDB->haveDecryptionShares( blockId, schain_index( 2 ) ) );

    // Stage 1: honest peers must still be accepted.
    for ( int idx = 3; idx <= 4; ++idx ) {
        CATCH_REQUIRE_NOTHROW(
            teDB->addDecryptionShares( makeMockupShareList( proposal, schain_index( idx ) ) ) );
        CATCH_REQUIRE( teDB->haveDecryptionShares( blockId, schain_index( idx ) ) );
    }

    // Stage 2: our own shares must be accepted and persisted, otherwise this node can no
    // longer serve them to its peers.
    CATCH_REQUIRE_NOTHROW(
        teDB->addMyDecryptionShares( makeMockupShareList( proposal, schain_index( 1 ) ) ) );
    CATCH_REQUIRE( teDB->haveDecryptionShares( blockId, schain_index( 1 ) ) );
    CATCH_REQUIRE( teDB->getMyDecryptionShares( blockId, proposerIndex ) != nullptr );
    CATCH_REQUIRE( teDB->getDecryptionsCount( blockId ) == 4 );

    // Stage 3: the merge must skip the empty list and decrypt with the three honest ones.
    uint64_t sharesUsed = 0;
    ptr<DecryptedAESKeyList> keys;
    CATCH_REQUIRE_NOTHROW( keys = teDB->mergeAESKeys(
                               blockId, proposal->getTransactionCiphertexts(), &sharesUsed ) );
    CATCH_REQUIRE( keys );
    CATCH_REQUIRE( sharesUsed == 4 );
    CATCH_REQUIRE( keys->totalDecryptedCiphertextsCount() ==
                   proposal->getTransactionCiphertexts()->totalCiphertextCount() );
}

#endif
