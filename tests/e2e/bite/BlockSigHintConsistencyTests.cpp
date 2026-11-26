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
#include "bite/BiteManager.h"
#include "chains/Schain.h"
#include "crypto/BLAKE3Hash.h"
#include "crypto/CryptoManager.h"
#include "crypto/DecryptedAESKeyList.h"
#include "datastructures/CommittedBlock.h"
#include "node/ConsensusEngine.h"
#include "node/Node.h"
#include "protocols/blockconsensus/ConsensusSignatureDomains.h"
#include "tests/TestHooks.h"
#include "tests/e2e/ConsensusEngineTestAccess.h"
#include "tests/e2e/E2ETestHelper.h"
#include "utils/Time.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace BlockSigHintConsistencyTests {

using namespace std::chrono_literals;
using E2EHelper = E2ETestUtils::E2ETestHelper;
using Chain = E2ETestUtils::FourNodeTestChain;

// In some tests this node changes the signature shares it receives before storing them.
static const node_id MODIFIED_NODE = Chain::NODE_2;

// All nodes except MODIFIED_NODE.
static const std::vector< node_id > UNCHANGED_NODES = [] {
    std::vector< node_id > nodes;
    for ( auto nodeId : Chain::ALL_NODES )
        if ( nodeId != MODIFIED_NODE )
            nodes.push_back( nodeId );
    return nodes;
}();

static constexpr auto RUN_TIMEOUT = 150s;
static constexpr auto POLL_INTERVAL = 100ms;

// Blocks every node must commit before the checks start.
static constexpr uint64_t MIN_BLOCKS = 6;

// Blocks the catchup test checks when the patch is off.
static constexpr uint64_t CATCHUP_CHECKED_BLOCKS = 5;

// Seconds from chain start until the patch turns on, in tests that run across activation.
static constexpr uint64_t ACTIVATION_DELAY_S = 30;

// Patch timestamps: 0 = off, 1 = on from the first block.
static constexpr uint64_t NO_ACTIVATION = 0;
static constexpr uint64_t ACTIVE_FROM_START = 1;

// The two threshold signatures in a block:
// Normal: signs the block hash; the block random comes from it.
// Reencryption: signs the block hash with the reencryption domain; the reencryption random comes
// from it.
enum class SigKind { Normal, Reencryption };

// Block 1 has no reencryption signature.
static uint64_t firstBlockWithSig( SigKind _kind ) {
    return _kind == SigKind::Normal ? 1 : 2;
}


// ---------- Starting the chain and reading blocks from each node ----------

// Starts the chain with the patch timestamp _sigPatchTimestamp and waits until every node has
// MIN_BLOCKS blocks. Returns the last block all nodes have.
static uint64_t startAndWaitForBlocks(
    std::unique_ptr< ConsensusEngine >& _engine, uint64_t _sigPatchTimestamp ) {
    _engine = E2EHelper::startEngine(
        {
            .patchTimestamps = {
                { "bite2PatchTimestamp", ACTIVE_FROM_START },
                { "protocolHardeningV1PatchTimestamp", _sigPatchTimestamp }
            },
            .requireSgx = true
        }
    );
    uint64_t lastBlock = 0;
    CATCH_REQUIRE( E2ETestUtils::becomesTrueWithin( RUN_TIMEOUT, POLL_INTERVAL, [&] {
        lastBlock = ConsensusEngineTestAccess::getLastBlockCommittedByAll( *_engine );
        return lastBlock >= MIN_BLOCKS;
    } ) );
    
    return lastBlock;
}

// Result of a run in which the patch turns on mid-run.
struct ActivationRun {
    uint64_t activationS;  // when the patch turns on, in seconds
    uint64_t firstActive;  // first block with a timestamp at or after activationS
    uint64_t lastBlock;    // last block all nodes have
};

// Starts the chain with the patch turning on ACTIVATION_DELAY_S seconds later and waits for three
// blocks after activation. Fails unless a block with a _kind signature came before activation.
static ActivationRun runAcrossActivation(
    std::unique_ptr< ConsensusEngine >& _engine, SigKind _kind ) {
    ActivationRun run{};
    const uint64_t setupStartMs = Time::getCurrentTimeMs();
    // Now plus ACTIVATION_DELAY_S, rounded up to a whole second.
    run.activationS = ( setupStartMs + ACTIVATION_DELAY_S * 1000 + 999 ) / 1000;
    _engine = E2EHelper::startEngine(
        {
            .patchTimestamps = {
                { "bite2PatchTimestamp", ACTIVE_FROM_START },
                { "protocolHardeningV1PatchTimestamp", run.activationS }
            },
            .requireSgx = true
        }
    );
    const uint64_t setupMs = Time::getCurrentTimeMs() - setupStartMs;
    CATCH_INFO( "patch turns on at " << run.activationS << " s; engine setup took " << setupMs
                                      << " ms of the " << ACTIVATION_DELAY_S << " s delay" );
    // Timestamps never decrease, so if the third-last block is at or after activation, so are
    // the last three.
    CATCH_REQUIRE( E2ETestUtils::becomesTrueWithin( RUN_TIMEOUT, POLL_INTERVAL, [&] {
        run.lastBlock = ConsensusEngineTestAccess::getLastBlockCommittedByAll( *_engine );
        if ( run.lastBlock < 3 )
            return false;
        auto thirdLast = ConsensusEngineTestAccess::getCommittedBlockForBlockIdForNode(
            *_engine, run.lastBlock - 2, Chain::NODE_1 );
        return thirdLast->getTimeStampS() >= run.activationS;
    } ) );
    // Walk back to the first block at or after activation.
    run.firstActive = run.lastBlock - 2;
    while ( run.firstActive > 1 ) {
        auto previous = ConsensusEngineTestAccess::getCommittedBlockForBlockIdForNode(
            *_engine, run.firstActive - 1, Chain::NODE_1 );
        if ( previous->getTimeStampS() < run.activationS )
            break;
        run.firstActive--;
    }
    // If this fails, no signed block came before activation: raise ACTIVATION_DELAY_S.
    auto firstBlock = ConsensusEngineTestAccess::getCommittedBlockForBlockIdForNode(
        *_engine, 1, Chain::NODE_1 );
    CATCH_INFO( "first block at or after activation: " << run.firstActive
                << "; block 1 timestamp: " << firstBlock->getTimeStampS() << " s" );
    CATCH_REQUIRE( run.firstActive > firstBlockWithSig( _kind ) );
    return run;
}


// ---------- Expected signature text and checks against it ----------

// The text of the _kind signature stored in _block.
static std::string sigOf( const ptr< CommittedBlock >& _block, SigKind _kind ) {
    if ( _kind == SigKind::Normal )
        return _block->getThresholdSig();
    auto sig = _block->getReencryptionThresholdSig();
    CATCH_REQUIRE( sig.has_value() );
    return *sig;
}

// The hash that the _kind signature of _block signs.
static BLAKE3Hash sigHash( const ptr< CommittedBlock >& _block, SigKind _kind ) {
    auto hash = BLAKE3Hash::getConsensusHash( ( uint64_t ) _block->getProposerIndex(),
        ( uint64_t ) _block->getBlockID(), ( uint64_t ) _block->getSchainID() );
    if ( _kind == SigKind::Normal )
        return hash;
    return BLAKE3Hash::getDomainHash( hash, blockconsensus::REENCRYPTION_RANDOM_DOMAIN );
}

// The random that node _nodeId derives from the _kind signature of block _blockId.
static u256 randomOf(
    const ConsensusEngine& _engine, node_id _nodeId, uint64_t _blockId, SigKind _kind ) {
    if ( _kind == SigKind::Normal )
        return ConsensusEngineTestAccess::getNode( _engine, _nodeId )
            ->getSchain()
            ->getRandomForBlockId( block_id( _blockId ) );
    return _engine.getReencryptionRandomForBlockIdForNode( _blockId, _nodeId );
}

// A signature is stored as text with four fields: "X:Y:hintY:counter".
using SigTokens = std::array< std::string, 4 >;

// Splits signature text into its four fields. Fails unless there are exactly four, none empty.
static SigTokens splitSigString( const std::string& _s ) {
    CATCH_INFO( "Signature string: " << _s );
    CATCH_REQUIRE( std::count( _s.begin(), _s.end(), ':' ) == 3 );
    SigTokens tokens;
    size_t start = 0;
    for ( auto& token : tokens ) {
        auto end = _s.find( ':', start );
        token = _s.substr( start, end - start );
        CATCH_REQUIRE( !token.empty() );
        start = end + 1;
    }
    return tokens;
}

// For each block from _first to _last, requires every node in _nodes to store the same canonical
// _kind signature text, derive the same random and have the same timestamp. Compares text only;
// does not verify the signatures.
static void requireNodesAgreeOnCanonicalSig( const ConsensusEngine& _engine, SigKind _kind,
    const std::vector< node_id >& _nodes, uint64_t _first, uint64_t _last ) {
    CATCH_REQUIRE( !_nodes.empty() );
    for ( uint64_t blockId = _first; blockId <= _last; blockId++ ) {
        auto reference = ConsensusEngineTestAccess::getCommittedBlockForBlockIdForNode(
            _engine, blockId, _nodes.front() );
        const auto referenceSig = sigOf( reference, _kind );
        // Mockup signatures have no ':' and pass through canonicalBlsSigString unchanged, so
        // require the four BLS fields.
        splitSigString( referenceSig );
        auto expectedSig =
            CryptoManager::canonicalBlsSigString( referenceSig, sigHash( reference, _kind ) );
        auto expectedRandom = randomOf( _engine, _nodes.front(), blockId, _kind );
        CATCH_REQUIRE( expectedRandom != u256( 0 ) );
        for ( auto nodeId : _nodes ) {
            CATCH_INFO( "block " << blockId << " node " << nodeId << " proposer index "
                                 << ( uint64_t ) reference->getProposerIndex() );
            auto block = ConsensusEngineTestAccess::getCommittedBlockForBlockIdForNode(
                _engine, blockId, nodeId );
            CATCH_REQUIRE( block->getTimeStampS() == reference->getTimeStampS() );
            CATCH_REQUIRE( sigOf( block, _kind ) == expectedSig );
            CATCH_REQUIRE( randomOf( _engine, nodeId, blockId, _kind ) == expectedRandom );
        }
    }
}


// ---------- Test hooks for MODIFIED_NODE ----------

// Share store hook: adds a leading zero to the counter of every _kind share _nodeId stores
// ("...:5" becomes "...:05"). The value stays the same; only the text changes. Without the patch,
// merging copies this hint into the stored signature. Counts changed shares in _modified.
static std::function< void( node_id, schain_index, bool, std::string& ) > zeroPadSharesSavedBy(
    node_id _nodeId, SigKind _kind, std::atomic< uint64_t >& _modified ) {
    const bool offchainDomain = _kind == SigKind::Reencryption;
    return [_nodeId, offchainDomain, &_modified]( node_id _receiverNodeId, schain_index,
               bool _shareOffchainDomain, std::string& _shareString ) {
        if ( _receiverNodeId != _nodeId || _shareOffchainDomain != offchainDomain )
            return;
        auto pos = _shareString.rfind( ':' );
        if ( pos == std::string::npos )
            return;
        _shareString.insert( pos + 1, "0" );
        _modified.fetch_add( 1 );
    };
}

// Catchup hook that keeps _isolatedNode building its own signatures, and keeps its blocks away
// from the other nodes.
static std::function< void( node_id, node_id, block_id, bool& ) > isolateFromCatchup(
    node_id _isolatedNode ) {
    auto refuseCatchupBeforeFirstBlock = E2ETestUtils::suppressInitialCatchup( _isolatedNode );
    // Runs for every catchup request: _requestingNode asks _answeringNode for the blocks after
    // _afterBlockId.
    return [_isolatedNode, refuseCatchupBeforeFirstBlock]( node_id _answeringNode,
               node_id _requestingNode, block_id _afterBlockId, bool& _suppress ) {
        // Refuse _isolatedNode's requests until it has its first block.
        refuseCatchupBeforeFirstBlock(
            _answeringNode, _requestingNode, _afterBlockId, _suppress );
        // Refuse all requests that _isolatedNode would answer.
        if ( _answeringNode == _isolatedNode )
            _suppress = true;
    };
}

// Runs the chain across activation while MODIFIED_NODE stores zero-padded _kind shares. Checks:
// - before activation, the other nodes store the canonical text and MODIFIED_NODE stores other
//   text for at least one block, which shows the hook takes effect;
// - after activation, every node stores the canonical text.
static void checkModifiedNodeAcrossActivation(
    std::unique_ptr< ConsensusEngine >& _engine, SigKind _kind ) {

    std::atomic< uint64_t > modifiedShares{ 0 };
    TestHooks::BlockSigShareStoreHook::Scope modifyScope(
        zeroPadSharesSavedBy( MODIFIED_NODE, _kind, modifiedShares ) );
    TestHooks::BlockCatchupResponseHook::Scope catchupScope(
        isolateFromCatchup( MODIFIED_NODE ) );

    const ActivationRun run = runAcrossActivation( _engine, _kind );
    CATCH_REQUIRE( modifiedShares.load() > 0 );

    const uint64_t first = firstBlockWithSig( _kind );
    const uint64_t lastBefore = run.firstActive - 1;

    // all nodes but MODIFIED_NODE agree
    requireNodesAgreeOnCanonicalSig( *_engine, _kind, UNCHANGED_NODES, first, lastBefore );

    // confirm mismatch between MODIFIED_NODE and one other node
    bool storedOtherText = false;
    for ( uint64_t blockId = first; blockId <= lastBefore && !storedOtherText; blockId++ ) {
        auto modified = ConsensusEngineTestAccess::getCommittedBlockForBlockIdForNode(
            *_engine, blockId, MODIFIED_NODE );
        auto reference = ConsensusEngineTestAccess::getCommittedBlockForBlockIdForNode(
            *_engine, blockId, Chain::NODE_1 );
        storedOtherText = sigOf( modified, _kind ) != sigOf( reference, _kind );
    }
    CATCH_REQUIRE( storedOtherText );

    // all nodes agree after patch is active
    requireNodesAgreeOnCanonicalSig( *_engine, _kind, Chain::ALL_NODES, run.firstActive, run.lastBlock );
}


// ---------- Catchup test: does a node accept a block whose signature text was changed? ----------

// Error texts that show which check rejected a catchup block.
static const char* const BLOCK_SIG_FORMAT_ERROR = "Unexpected block signature format";
static const char* const REENCRYPTION_SIG_FORMAT_ERROR = "Unexpected reencryption signature format";
// Both BLS verification failures in CryptoManager::verifyThresholdSig contain this text.
static const char* const SIG_VERIFICATION_ERROR = "BLS sig";

// A changed signature text, and the error a block after activation must be rejected with.
struct SigVariant {
    std::string name;
    std::string sig;
    std::string expectedError;
};

// Changed versions of the _kind signature text of _block. Most write the same values differently;
// "other counter" changes the counter value, and "block signature as reencryption signature" is a
// valid signature over the wrong hash.
static std::vector< SigVariant > sigVariants( const ptr< CommittedBlock >& _block, SigKind _kind ) {
    const auto tokens = splitSigString( sigOf( _block, _kind ) );
    enum Field : size_t { X, Y, HINT, COUNTER };
    // The signature text with field _field replaced by _value.
    auto withField = [&tokens]( Field _field, const std::string& _value ) {
        auto changed = tokens;
        changed[_field] = _value;
        return changed[X] + ":" + changed[Y] + ":" + changed[HINT] + ":" + changed[COUNTER];
    };
    auto zeroPadded = [&]( Field _field ) { return withField( _field, "0" + tokens[_field] ); };

    if ( _kind == SigKind::Reencryption ) {
        return {
            { "block signature as reencryption signature", _block->getThresholdSig(),
                SIG_VERIFICATION_ERROR },
            { "zero-padded counter", zeroPadded( COUNTER ), REENCRYPTION_SIG_FORMAT_ERROR },
        };
    }

    const auto otherCounter = std::to_string( std::stoull( tokens[COUNTER] ) + 1 );
    return {
        { "zero-padded X", zeroPadded( X ), BLOCK_SIG_FORMAT_ERROR },
        { "zero-padded Y", zeroPadded( Y ), BLOCK_SIG_FORMAT_ERROR },
        { "zero-padded hint", zeroPadded( HINT ), BLOCK_SIG_FORMAT_ERROR },
        { "zero-padded counter", zeroPadded( COUNTER ), BLOCK_SIG_FORMAT_ERROR },
        { "empty token", withField( X, tokens[X] + ":" ), BLOCK_SIG_FORMAT_ERROR },
        { "other counter", withField( COUNTER, otherCounter ), BLOCK_SIG_FORMAT_ERROR },
    };
}

// Reads copies of node 1's block _blockId as catchup blocks, one per sigVariants entry. If the
// block is at or after activation, every copy must be rejected with its expected error;
// otherwise every copy must be accepted.
static void checkCatchupOfChangedSignatures(
    const ConsensusEngine& _engine, uint64_t _blockId, uint64_t _activationS, SigKind _kind ) {
    auto schain = ConsensusEngineTestAccess::getNode( _engine, Chain::NODE_1 )->getSchain();
    auto cryptoManager = schain->getCryptoManager();
    auto biteManager = schain->getBiteManager();
    CATCH_REQUIRE( cryptoManager );
    CATCH_REQUIRE( biteManager );

    auto block = ConsensusEngineTestAccess::getCommittedBlockForBlockIdForNode(
        _engine, _blockId, Chain::NODE_1 );
    auto storedKeys = block->getDecryptedAesKeyList();
    CATCH_REQUIRE( storedKeys );
    // CommittedBlock::make needs a non-const key list.
    auto aesKeys = std::make_shared< DecryptedAESKeyList >( *storedKeys );
    const bool mustBeRejected =
        _activationS != NO_ACTIVATION && block->getTimeStampS() >= _activationS;

    // Rebuilds the block with _sig as its _kind signature and reads it as a catchup block.
    // Returns the error text, or "" if the block is accepted.
    auto catchupError = [&]( const std::string& _sig ) -> std::string {
        auto thresholdSig = block->getThresholdSig();
        auto reencryptionSig = block->getReencryptionThresholdSig();
        if ( _kind == SigKind::Normal ) {
            thresholdSig = _sig;
        } else {
            CATCH_REQUIRE( reencryptionSig.has_value() );
            reencryptionSig = _sig;
        }
        auto rebuilt = CommittedBlock::make( block->getSchainID(), block->getProposerNodeID(),
            block->getBlockID(), block->getEpochID(), block->getProposerIndex(),
            block->getTransactionList(), block->getStateRoot(), block->getTimeStampS(),
            block->getTimeStampMs(), block->getSignature(), thresholdSig, reencryptionSig,
            block->getDaSig(), aesKeys, block->getDecryptedTransactions() );
        // Only deserialize errors count as rejection; build errors fail the test.
        auto serialized = rebuilt->serialize();
        try {
            CommittedBlock::deserialize( serialized, cryptoManager, biteManager, true );
            return "";
        } catch ( const std::exception& e ) {
            return E2ETestUtils::nestedExceptionMessages( e );
        } catch ( ... ) {
            return "unknown exception";
        }
    };

    CATCH_INFO( "block " << _blockId << " timestamp " << block->getTimeStampS() << " activation "
                         << _activationS );

    // An unchanged copy must always be accepted.
    CATCH_REQUIRE( catchupError( sigOf( block, _kind ) ) == "" );

    for ( const auto& variant : sigVariants( block, _kind ) ) {
        CATCH_INFO( "variant: " << variant.name << ", signature " << variant.sig );
        const auto error = catchupError( variant.sig );
        CATCH_INFO( "catchup error: " << ( error.empty() ? "none" : error ) );
        CATCH_REQUIRE( !error.empty() == mustBeRejected );
        if ( mustBeRejected ) {
            CATCH_INFO( "expected error: " << variant.expectedError );
            CATCH_REQUIRE( error.find( variant.expectedError ) != std::string::npos );
        }
    }
}

// Runs the chain across activation and checks catchup of changed _kind signatures for the two
// blocks before activation and the first three after it.
static void checkCatchupAroundActivation(
    std::unique_ptr< ConsensusEngine >& _engine, SigKind _kind ) {
    const auto run = runAcrossActivation( _engine, _kind );
    // Skips blocks without a _kind signature. runAcrossActivation ensures firstActive >= 2 and
    // firstActive + 2 <= lastBlock.
    for ( uint64_t blockId = std::max( run.firstActive - 2, firstBlockWithSig( _kind ) );
          blockId <= run.firstActive + 2; blockId++ )
        checkCatchupOfChangedSignatures( *_engine, blockId, run.activationS, _kind );
}

}  // namespace BlockSigHintConsistencyTests

using namespace BlockSigHintConsistencyTests;


CATCH_TEST_CASE( "Block signature text is canonical and identical on all nodes",
    "[bite][sgx][block-signature][end-to-end]" ) {
    E2EHelper::configureTestEnvironment( true, Chain::CONFIG_DIR );

    std::unique_ptr< ConsensusEngine > testEngine;

    CATCH_SECTION( "Patch on from start: node with non-canonical shares stores canonical text" ) {
        // MODIFIED_NODE stores the shares it receives with non-canonical hints.
        std::atomic< uint64_t > modifiedShares{ 0 };
        TestHooks::BlockSigShareStoreHook::Scope modifyScope(
            zeroPadSharesSavedBy( MODIFIED_NODE, SigKind::Normal, modifiedShares ) );
        TestHooks::BlockCatchupResponseHook::Scope catchupScope(
            isolateFromCatchup( MODIFIED_NODE ) );

        const uint64_t lastBlock = startAndWaitForBlocks( testEngine, ACTIVE_FROM_START );
        CATCH_REQUIRE( modifiedShares.load() > 0 );

        // MODIFIED_NODE still stores the canonical text after merging.
        requireNodesAgreeOnCanonicalSig( *testEngine, SigKind::Normal, Chain::ALL_NODES, 1, lastBlock );
    }

    CATCH_SECTION( "Patch off: honest nodes already store canonical text" ) {
        const uint64_t lastBlock = startAndWaitForBlocks( testEngine, NO_ACTIVATION );
        requireNodesAgreeOnCanonicalSig( *testEngine, SigKind::Normal, Chain::ALL_NODES, 1, lastBlock );
    }

    CATCH_SECTION("Activation mid-run: node with non-canonical shares switches to canonical text" ) {
        // MODIFIED_NODE stores other text before activation and the canonical text after.
        checkModifiedNodeAcrossActivation( testEngine, SigKind::Normal );
    }

    CATCH_SUCCEED();
}

CATCH_TEST_CASE( "Non-canonical block signature is rejected on catchup after activation",
    "[bite][sgx][block-signature][catchup][end-to-end]" ) {
    E2EHelper::configureTestEnvironment( true, Chain::CONFIG_DIR );

    std::unique_ptr< ConsensusEngine > testEngine;

    CATCH_SECTION( "Activation mid-run: changed signatures are rejected only after activation" ) {
        checkCatchupAroundActivation( testEngine, SigKind::Normal );
    }

    CATCH_SECTION( "Patch off: changed signatures are accepted" ) {
        const uint64_t lastBlock = startAndWaitForBlocks( testEngine, NO_ACTIVATION );
        const uint64_t lastChecked = std::min( lastBlock, CATCHUP_CHECKED_BLOCKS );
        for ( uint64_t blockId = 1; blockId <= lastChecked; blockId++ )
            checkCatchupOfChangedSignatures( *testEngine, blockId, NO_ACTIVATION, SigKind::Normal );
    }

    CATCH_SUCCEED();
}

// The same checks as above, for the reencryption signature and the reencryption random.
CATCH_TEST_CASE( "Reencryption signature is canonical on all nodes and verified on catchup",
    "[bite][sgx][block-signature][reencryption][end-to-end]" ) {
    E2EHelper::configureTestEnvironment( true, Chain::CONFIG_DIR );

    std::unique_ptr< ConsensusEngine > testEngine;

    CATCH_SECTION("Activation mid-run: node with non-canonical shares switches to canonical text" ) {
        checkModifiedNodeAcrossActivation( testEngine, SigKind::Reencryption );
    }

    CATCH_SECTION("Activation mid-run: changed or wrong-hash signatures rejected only after activation" ) {
        checkCatchupAroundActivation( testEngine, SigKind::Reencryption );
    }

    CATCH_SUCCEED();
}

#endif  // BITE
