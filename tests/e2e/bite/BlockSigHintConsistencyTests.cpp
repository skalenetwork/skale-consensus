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
#include "tests/harness/transport/SocketIO.h"
#include "utils/Time.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace BlockSigHintConsistencyTests {

using namespace std::chrono_literals;
using E2EHelper = E2ETestUtils::E2ETestHelper;
using Chain = E2ETestUtils::FourNodeTestChain;
using E2ETestUtils::WaitResult;
using E2ETestUtils::deadlineIn;
using E2ETestUtils::waitUntil;

static const std::vector< node_id > ALL_NODES = {
    Chain::NODE_1, Chain::NODE_2, Chain::NODE_3, Chain::NODE_4 };

// In some tests this node rewrites the signature shares it receives before saving them.
static const node_id MODIFIED_NODE = Chain::NODE_2;

// All nodes except MODIFIED_NODE.
static const std::vector< node_id > UNCHANGED_NODES = [] {
    std::vector< node_id > nodes;
    for ( auto nodeId : ALL_NODES )
        if ( nodeId != MODIFIED_NODE )
            nodes.push_back( nodeId );
    return nodes;
}();

static constexpr auto RUN_TIMEOUT = 150s;
static constexpr auto POLL_INTERVAL = 100ms;

// How many blocks every node must commit before the test starts checking.
static constexpr uint64_t MIN_BLOCKS = 6;

// How many blocks the catchup test checks when the patch is off.
static constexpr uint64_t CATCHUP_CHECKED_BLOCKS = 5;

// In the tests where the patch turns on during the run, it turns on this many seconds after
// the chain starts, so that some blocks are committed before it and some after.
static constexpr uint64_t ACTIVATION_DELAY_S = 30;

// Name of the patch under test. When it is on, every node must store the block signatures in
// one exact text format.
static const char* const SIG_PATCH_KEY = "protocolHardeningV1PatchTimestamp";

// Patch timestamp values: 0 means the patch is off, 1 means it is on from the first block.
static constexpr uint64_t NO_ACTIVATION = 0;
static constexpr uint64_t ACTIVE_FROM_START = 1;

// The two threshold signatures a block carries. The tests check both the same way.
enum class SigKind { Block, Reencryption };

// Block 1 has no reencryption signature, so reencryption checks start at block 2.
static uint64_t firstBlockWithSig( SigKind _kind ) {
    return _kind == SigKind::Block ? 1 : 2;
}


// ---------- Starting the chain and reading blocks from each node ----------

static std::map< std::string, uint64_t > patchMap( uint64_t _sigPatchTimestamp ) {
    return { { "bite2PatchTimestamp", ACTIVE_FROM_START }, { SIG_PATCH_KEY, _sigPatchTimestamp } };
}

static uint64_t activationInSeconds( uint64_t _delayS ) {
    return ( Time::getCurrentTimeMs() + _delayS * 1000 + 999 ) / 1000;
}

static void setupFourNodeEngine( ConsensusEngine*& _engine, uint64_t _sigPatchTimestamp ) {
    E2EHelper::setupEngine( _engine, 0, patchMap( _sigPatchTimestamp ) );
    for ( auto nodeId : ALL_NODES ) {
        CATCH_INFO( "SGX test keys are required for node " << nodeId );
        CATCH_REQUIRE( ConsensusEngineTestAccess::getNode( *_engine, nodeId )->isSgxEnabled() );
    }
    _engine->slowStartBootStrapTest();
}

static uint64_t minLastCommitted( const ConsensusEngine& _engine ) {
    uint64_t result = std::numeric_limits< uint64_t >::max();
    for ( auto nodeId : ALL_NODES ) {
        result = std::min( result,
            static_cast< uint64_t >(
                ConsensusEngineTestAccess::getCurrentLastCommittedBlockIDForNode(
                    _engine, nodeId ) ) );
    }
    return result;
}

static uint64_t minLastCommittedTimeStampS( const ConsensusEngine& _engine ) {
    uint64_t result = std::numeric_limits< uint64_t >::max();
    for ( auto nodeId : ALL_NODES ) {
        result = std::min( result,
            ConsensusEngineTestAccess::getCurrentLastCommittedBlockTimeStampForNode(
                _engine, nodeId )
                .getS() );
    }
    return result;
}

static ptr< CommittedBlock > blockOf(
    const ConsensusEngine& _engine, node_id _nodeId, uint64_t _blockId ) {
    return ConsensusEngineTestAccess::getCommittedBlockForBlockIdForNode(
        _engine, _blockId, _nodeId );
}

// Returns the first block with a timestamp at or after _timeStampS, or 0 if there is none.
static uint64_t firstBlockAtOrAfter(
    const ConsensusEngine& _engine, uint64_t _timeStampS, uint64_t _last ) {
    for ( uint64_t blockId = 1; blockId <= _last; blockId++ ) {
        if ( blockOf( _engine, Chain::NODE_1, blockId )->getTimeStampS() >= _timeStampS )
            return blockId;
    }
    return 0;
}

// Checks _condition every POLL_INTERVAL. Fails the test if it is still false after RUN_TIMEOUT.
template < typename Condition >
static void waitOrFail( Condition _condition ) {
    CATCH_REQUIRE( waitUntil( _condition, deadlineIn( RUN_TIMEOUT ), POLL_INTERVAL ) ==
                   WaitResult::Satisfied );
}

// Starts the chain and waits until every node has committed MIN_BLOCKS blocks. Returns the
// highest block that all nodes have committed.
static uint64_t startAndWaitForBlocks( ConsensusEngine*& _engine, uint64_t _sigPatchTimestamp ) {
    setupFourNodeEngine( _engine, _sigPatchTimestamp );
    waitOrFail( [&] { return minLastCommitted( *_engine ) >= MIN_BLOCKS; } );
    return minLastCommitted( *_engine );
}

// Result of a run in which the patch turns on while the chain is running.
struct ActivationRun {
    uint64_t activationS;  // time the patch turns on, in seconds
    uint64_t firstActive;  // first block with a timestamp at or after activationS
    uint64_t lastBlock;    // highest block that all nodes have committed
};

// Starts the chain with the patch turning on ACTIVATION_DELAY_S seconds later, and waits until
// every node has committed at least three blocks after that moment. Fails the test unless at
// least one block with a _kind signature was committed before the patch turned on.
static ActivationRun runAcrossActivation( ConsensusEngine*& _engine, SigKind _kind ) {
    ActivationRun run{};
    run.activationS = activationInSeconds( ACTIVATION_DELAY_S );
    const uint64_t setupStartMs = Time::getCurrentTimeMs();
    setupFourNodeEngine( _engine, run.activationS );
    const uint64_t setupMs = Time::getCurrentTimeMs() - setupStartMs;
    CATCH_INFO( "patch turns on at " << run.activationS << " s; engine setup took " << setupMs
                                      << " ms of the " << ACTIVATION_DELAY_S << " s delay" );
    // Only read blocks once every node is past the activation time, so the loop does not scan
    // the block database on every poll while the chain is still before it.
    waitOrFail( [&] {
        if ( minLastCommittedTimeStampS( *_engine ) < run.activationS )
            return false;
        run.lastBlock = minLastCommitted( *_engine );
        run.firstActive = firstBlockAtOrAfter( *_engine, run.activationS, run.lastBlock );
        return run.firstActive != 0 && run.firstActive + 2 <= run.lastBlock;
    } );
    // If this fails, the chain committed too few blocks before the patch turned on. Raise
    // ACTIVATION_DELAY_S.
    CATCH_INFO( "first block at or after activation: "
                << run.firstActive << "; block 1 timestamp: "
                << blockOf( *_engine, Chain::NODE_1, 1 )->getTimeStampS() << " s" );
    CATCH_REQUIRE( run.firstActive > firstBlockWithSig( _kind ) );
    return run;
}

// Returns the two blocks before the patch turned on and the first three blocks after it,
// skipping blocks without a _kind signature. runAcrossActivation guarantees firstActive >= 2.
static std::vector< uint64_t > blocksAroundActivation( const ActivationRun& _run, SigKind _kind ) {
    std::vector< uint64_t > result;
    for ( uint64_t blockId = _run.firstActive - 2; blockId <= _run.firstActive + 2; blockId++ ) {
        if ( blockId >= firstBlockWithSig( _kind ) && blockId <= _run.lastBlock )
            result.push_back( blockId );
    }
    return result;
}


// ---------- Expected signature text and checks against it ----------

// The text of the _kind signature stored in _block.
static std::string sigOf( const ptr< CommittedBlock >& _block, SigKind _kind ) {
    if ( _kind == SigKind::Block )
        return _block->getThresholdSig();
    auto sig = _block->getReencryptionThresholdSig();
    CATCH_REQUIRE( sig.has_value() );
    return *sig;
}

// The hash that the _kind signature of _block signs.
static BLAKE3Hash sigHash( const ptr< CommittedBlock >& _block, SigKind _kind ) {
    auto hash = BLAKE3Hash::getConsensusHash( ( uint64_t ) _block->getProposerIndex(),
        ( uint64_t ) _block->getBlockID(), ( uint64_t ) _block->getSchainID() );
    if ( _kind == SigKind::Block )
        return hash;
    return BLAKE3Hash::getDomainHash( hash, blockconsensus::REENCRYPTION_RANDOM_DOMAIN );
}

// The random that node _nodeId derives from the _kind signature of block _blockId.
static u256 randomOf(
    const ConsensusEngine& _engine, node_id _nodeId, uint64_t _blockId, SigKind _kind ) {
    if ( _kind == SigKind::Block )
        return ConsensusEngineTestAccess::getNode( _engine, _nodeId )
            ->getSchain()
            ->getRandomForBlockId( block_id( _blockId ) );
    return _engine.getReencryptionRandomForBlockIdForNode( _blockId, _nodeId );
}

// A signature is stored as text with four fields: "X:Y:hintY:counter".
using SigTokens = std::array< std::string, 4 >;

// Splits signature text into its four fields. Fails the test if the text does not have exactly
// four non-empty fields.
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

static std::string joinTokens( const SigTokens& _tokens ) {
    return _tokens[0] + ":" + _tokens[1] + ":" + _tokens[2] + ":" + _tokens[3];
}

// Returns the text every node should store for the _kind signature of _block. Fails the test
// unless the stored text is a BLS signature with four fields, so that a signature the canonical
// form leaves unchanged (one without separators) cannot pass the checks below.
static std::string canonicalSig( const ptr< CommittedBlock >& _block, SigKind _kind ) {
    const auto stored = sigOf( _block, _kind );
    splitSigString( stored );
    return CryptoManager::canonicalBlsSigString( stored, sigHash( _block, _kind ) );
}

// For every block from _first to _last, checks that each node in _nodes has the same block
// timestamp, stored the _kind signature in the expected text, and derived the same random from
// it.
static void checkSignaturesOnNodes( const ConsensusEngine& _engine, SigKind _kind,
    const std::vector< node_id >& _nodes, uint64_t _first, uint64_t _last ) {
    CATCH_REQUIRE( !_nodes.empty() );
    for ( uint64_t blockId = _first; blockId <= _last; blockId++ ) {
        auto reference = blockOf( _engine, _nodes.front(), blockId );
        auto expectedSig = canonicalSig( reference, _kind );
        auto expectedRandom = randomOf( _engine, _nodes.front(), blockId, _kind );
        CATCH_REQUIRE( expectedRandom != u256( 0 ) );
        for ( auto nodeId : _nodes ) {
            CATCH_INFO( "block " << blockId << " node " << nodeId << " proposer index "
                                 << ( uint64_t ) reference->getProposerIndex() );
            auto block = blockOf( _engine, nodeId, blockId );
            CATCH_REQUIRE( block->getTimeStampS() == reference->getTimeStampS() );
            CATCH_REQUIRE( sigOf( block, _kind ) == expectedSig );
            CATCH_REQUIRE( randomOf( _engine, nodeId, blockId, _kind ) == expectedRandom );
        }
    }
}

// Returns true if, for at least one block from _first to _last, MODIFIED_NODE stored different
// _kind signature text than node 1. MODIFIED_NODE may still receive some blocks from other nodes
// through catchup, and those blocks carry node 1's text, so the tests do not require every block
// to differ.
static bool modifiedNodeStoredOtherText(
    const ConsensusEngine& _engine, SigKind _kind, uint64_t _first, uint64_t _last ) {
    for ( uint64_t blockId = _first; blockId <= _last; blockId++ ) {
        if ( sigOf( blockOf( _engine, MODIFIED_NODE, blockId ), _kind ) !=
             sigOf( blockOf( _engine, Chain::NODE_1, blockId ), _kind ) )
            return true;
    }
    return false;
}


// ---------- Test hooks that make MODIFIED_NODE store shares in another format ----------

// Adds a leading zero to the last field (the counter), e.g. "...:5" becomes "...:05". The
// number does not change, only how it is written. Returns the text unchanged if it has no ':'.
// Does not use CATCH_REQUIRE, so it is safe to call from consensus threads.
static std::string zeroPadCounter( const std::string& _s ) {
    auto pos = _s.rfind( ':' );
    if ( pos == std::string::npos )
        return _s;
    return _s.substr( 0, pos + 1 ) + "0" + _s.substr( pos + 1 );
}

// Returns a hook that makes MODIFIED_NODE add a leading zero to the counter of every _kind
// signature share it saves, including its own. Counts the changed shares in _modified.
// The hook runs on consensus threads, so it only updates the atomic counter.
static std::function< void( node_id, schain_index, bool, std::string& ) >
zeroPadSharesOnModifiedNode( SigKind _kind, std::atomic< uint64_t >& _modified ) {
    const bool offchainDomain = _kind == SigKind::Reencryption;
    return [offchainDomain, &_modified]( node_id _receiverNodeId, schain_index,
               bool _shareOffchainDomain, std::string& _shareString ) {
        if ( _receiverNodeId != MODIFIED_NODE || _shareOffchainDomain != offchainDomain )
            return;
        auto padded = zeroPadCounter( _shareString );
        if ( padded != _shareString ) {
            _shareString = padded;
            _modified.fetch_add( 1 );
        }
    };
}

// Returns a hook that:
// - stops MODIFIED_NODE from downloading blocks at startup, so it has to build each block
//   signature itself from the shares it saved;
// - stops MODIFIED_NODE from sending blocks to other nodes, so its signature text never
//   reaches them.
static std::function< void( node_id, node_id, block_id, bool& ) > isolateModifiedNodeCatchup() {
    auto initial = E2ETestUtils::suppressInitialCatchup( MODIFIED_NODE );
    return [initial]( node_id _sender, node_id _receiver, block_id _afterBlockId,
               bool& _suppress ) {
        initial( _sender, _receiver, _afterBlockId, _suppress );
        if ( _sender == MODIFIED_NODE )
            _suppress = true;
    };
}

// MODIFIED_NODE stores the _kind signature shares it saves in another format while the chain
// runs across activation. Checks that:
// - before activation, the other nodes store the expected text and MODIFIED_NODE stores the
//   format it received, at least for the blocks it built itself. This shows the hook changed
//   the signatures MODIFIED_NODE builds;
// - after activation, every node, MODIFIED_NODE included, stores the expected text.
static void checkModifiedNodeAcrossActivation( ConsensusEngine*& _engine, SigKind _kind ) {
    std::atomic< uint64_t > modifiedShares{ 0 };
    TestHooks::BlockSigShareStoreHook::Scope modifyScope(
        zeroPadSharesOnModifiedNode( _kind, modifiedShares ) );
    TestHooks::BlockCatchupResponseHook::Scope catchupScope( isolateModifiedNodeCatchup() );

    const auto run = runAcrossActivation( _engine, _kind );
    CATCH_REQUIRE( modifiedShares.load() > 0 );

    const uint64_t first = firstBlockWithSig( _kind );
    const uint64_t lastBefore = run.firstActive - 1;
    checkSignaturesOnNodes( *_engine, _kind, UNCHANGED_NODES, first, lastBefore );
    CATCH_REQUIRE( modifiedNodeStoredOtherText( *_engine, _kind, first, lastBefore ) );

    checkSignaturesOnNodes( *_engine, _kind, ALL_NODES, run.firstActive, run.lastBlock );
}


// ---------- Catchup test: does a node accept a block whose signature text was changed? ----------

// Error texts that tell which check rejected a block received through catchup.
static const char* const BLOCK_SIG_FORMAT_ERROR = "Unexpected block signature format";
static const char* const REENCRYPTION_SIG_FORMAT_ERROR = "Unexpected reencryption signature format";
// Both BLS verification failures in CryptoManager::verifyThresholdSig contain this text.
static const char* const SIG_VERIFICATION_ERROR = "BLS sig";

// A named change to one signature of a block. The test puts the changed text into a copy of the
// block and offers it to a node through catchup. After the patch turns on, the copy must be
// rejected with an error that contains expectedError.
struct SigVariant {
    std::string name;
    std::string sig;
    std::string expectedError;
};

// Changes to the _kind signature text of _block. All but two only write the same values in
// another format: "other counter" changes the hint counter, and "block signature as reencryption
// signature" is a valid signature of the wrong hash.
static std::vector< SigVariant > sigVariants( const ptr< CommittedBlock >& _block, SigKind _kind ) {
    const auto [x, y, hintY, counter] = splitSigString( sigOf( _block, _kind ) );

    if ( _kind == SigKind::Reencryption ) {
        return {
            { "block signature as reencryption signature", _block->getThresholdSig(),
                SIG_VERIFICATION_ERROR },
            { "zero-padded counter", joinTokens( { x, y, hintY, "0" + counter } ),
                REENCRYPTION_SIG_FORMAT_ERROR },
        };
    }

    const auto otherCounter = std::to_string( std::stoull( counter ) + 1 );
    return {
        { "zero-padded X", joinTokens( { "0" + x, y, hintY, counter } ), BLOCK_SIG_FORMAT_ERROR },
        { "zero-padded Y", joinTokens( { x, "0" + y, hintY, counter } ), BLOCK_SIG_FORMAT_ERROR },
        { "zero-padded hint", joinTokens( { x, y, "0" + hintY, counter } ),
            BLOCK_SIG_FORMAT_ERROR },
        { "zero-padded counter", joinTokens( { x, y, hintY, "0" + counter } ),
            BLOCK_SIG_FORMAT_ERROR },
        { "empty token", x + "::" + y + ":" + hintY + ":" + counter, BLOCK_SIG_FORMAT_ERROR },
        { "other counter", joinTokens( { x, y, hintY, otherCounter } ), BLOCK_SIG_FORMAT_ERROR },
    };
}

// Returns the messages of _e and of every exception nested in it, one per line. Consensus code
// wraps exceptions with throw_with_nested, so the message of the check that failed is usually
// in a nested exception.
static std::string allMessages( const std::exception& _e ) {
    std::string result = _e.what();
    try {
        std::rethrow_if_nested( _e );
    } catch ( const std::exception& nested ) {
        result += "\n" + allMessages( nested );
    } catch ( ... ) {
        result += "\nunknown exception";
    }
    return result;
}

// Takes block _blockId from node 1, makes one copy per change from sigVariants, and reads each
// copy the way node 1 reads a block received through catchup (with signature checks).
// If the block was committed after the patch turned on, every changed copy must be rejected,
// with an error that shows the expected check rejected it. Otherwise every changed copy must be
// accepted, as before the patch.
static void checkCatchupOfChangedSignatures(
    const ConsensusEngine& _engine, uint64_t _blockId, uint64_t _activationS, SigKind _kind ) {
    auto schain = ConsensusEngineTestAccess::getNode( _engine, Chain::NODE_1 )->getSchain();
    auto cryptoManager = schain->getCryptoManager();
    auto biteManager = schain->getBiteManager();
    CATCH_REQUIRE( cryptoManager );
    CATCH_REQUIRE( biteManager );

    auto block = blockOf( _engine, Chain::NODE_1, _blockId );
    auto storedKeys = block->getDecryptedAesKeyList();
    CATCH_REQUIRE( storedKeys );
    // CommittedBlock::make takes a non-const key list, so the copies get their own copy of it.
    auto aesKeys = std::make_shared< DecryptedAESKeyList >( *storedKeys );
    const bool mustBeRejected =
        _activationS != NO_ACTIVATION && block->getTimeStampS() >= _activationS;

    // Builds a copy of the block with _sig as its _kind signature and reads it as a catchup
    // block. Returns the error messages if the copy is rejected, or "" if it is accepted.
    auto catchupError = [&]( const std::string& _sig ) -> std::string {
        auto thresholdSig = block->getThresholdSig();
        auto reencryptionSig = block->getReencryptionThresholdSig();
        if ( _kind == SigKind::Block ) {
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
        // Errors while building the copy fail the test; only reading it back counts as rejection.
        auto serialized = rebuilt->serialize();
        try {
            CommittedBlock::deserialize( serialized, cryptoManager, biteManager, true );
            return "";
        } catch ( const std::exception& e ) {
            return allMessages( e );
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

// Runs the chain across activation and checks catchup of changed _kind signatures for the
// blocks just before and just after activation.
static void checkCatchupAroundActivation( ConsensusEngine*& _engine, SigKind _kind ) {
    const auto run = runAcrossActivation( _engine, _kind );
    for ( auto blockId : blocksAroundActivation( run, _kind ) )
        checkCatchupOfChangedSignatures( *_engine, blockId, run.activationS, _kind );
}

}  // namespace BlockSigHintConsistencyTests

using namespace BlockSigHintConsistencyTests;

// With the patch on, every node stores the same block signature text and computes the same
// block random, even if one node received its signature shares in another format.
CATCH_TEST_CASE( "Block signature string is canonical and identical on all nodes",
    "[bite][sgx][block-signature][end-to-end]" ) {
    E2EHelper::configureTestEnvironment( true, Chain::CONFIG_DIR );

    ConsensusEngine* testEngine = nullptr;
    E2ETestUtils::ScopedEngineShutdown engineShutdown( testEngine );

    CATCH_SECTION( "activation from the start, modified share encoding on one node" ) {
        std::atomic< uint64_t > modifiedShares{ 0 };
        TestHooks::BlockSigShareStoreHook::Scope modifyScope(
            zeroPadSharesOnModifiedNode( SigKind::Block, modifiedShares ) );
        TestHooks::BlockCatchupResponseHook::Scope catchupScope( isolateModifiedNodeCatchup() );

        const uint64_t lastBlock = startAndWaitForBlocks( testEngine, ACTIVE_FROM_START );
        CATCH_REQUIRE( modifiedShares.load() > 0 );
        checkSignaturesOnNodes( *testEngine, SigKind::Block, ALL_NODES, 1, lastBlock );
    }

    CATCH_SECTION( "unmodified run without activation keeps the canonical bytes" ) {
        const uint64_t lastBlock = startAndWaitForBlocks( testEngine, NO_ACTIVATION );
        checkSignaturesOnNodes( *testEngine, SigKind::Block, ALL_NODES, 1, lastBlock );
    }

    CATCH_SECTION( "activation in the future, modified share encoding on one node" ) {
        checkModifiedNodeAcrossActivation( testEngine, SigKind::Block );
    }

    CATCH_SUCCEED();
}

// For blocks committed after the patch turned on, a node must reject a block received through
// catchup if its signature text differs from the expected text. Older blocks are accepted.
CATCH_TEST_CASE( "Non-canonical block signature string is rejected on catchup",
    "[bite][sgx][block-signature][catchup][end-to-end]" ) {
    E2EHelper::configureTestEnvironment( true, Chain::CONFIG_DIR );

    ConsensusEngine* testEngine = nullptr;
    E2ETestUtils::ScopedEngineShutdown engineShutdown( testEngine );

    CATCH_SECTION( "activation in the future" ) {
        checkCatchupAroundActivation( testEngine, SigKind::Block );
    }

    CATCH_SECTION( "no activation" ) {
        const uint64_t lastBlock = startAndWaitForBlocks( testEngine, NO_ACTIVATION );
        const uint64_t lastChecked = std::min( lastBlock, CATCHUP_CHECKED_BLOCKS );
        for ( uint64_t blockId = 1; blockId <= lastChecked; blockId++ )
            checkCatchupOfChangedSignatures( *testEngine, blockId, NO_ACTIVATION, SigKind::Block );
    }

    CATCH_SUCCEED();
}

// The same checks as above, for the reencryption signature and the reencryption random.
CATCH_TEST_CASE( "Reencryption signature string is canonical and verified",
    "[bite][sgx][block-signature][reencryption][end-to-end]" ) {
    E2EHelper::configureTestEnvironment( true, Chain::CONFIG_DIR );

    ConsensusEngine* testEngine = nullptr;
    E2ETestUtils::ScopedEngineShutdown engineShutdown( testEngine );

    CATCH_SECTION( "reencryption random is identical on all nodes" ) {
        checkModifiedNodeAcrossActivation( testEngine, SigKind::Reencryption );
    }

    CATCH_SECTION( "reencryption signature on catchup" ) {
        checkCatchupAroundActivation( testEngine, SigKind::Reencryption );
    }

    CATCH_SUCCEED();
}

#endif  // BITE
