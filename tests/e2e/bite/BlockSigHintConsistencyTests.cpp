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
#include "crypto/bls_include.h"
#include "libBLS/bls/BLSSignature.h"
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

// The chain has 4 nodes and a signature needs shares from 3 of them.
static constexpr size_t TOTAL_SIGNERS = 4;
static constexpr size_t REQUIRED_SIGNERS = 3;

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

// Block 1 has no reencryption signature, so reencryption checks start at block 2.
static constexpr uint64_t FIRST_REENCRYPTION_BLOCK_ID = 2;

// Name of the patch under test. When it is on, every node must store the block signature in
// one exact text format.
static const char* const SIG_PATCH_KEY = "protocolHardeningV1PatchTimestamp";

// Patch timestamp values: 0 means the patch is off, 1 means it is on from the first block.
static constexpr uint64_t NO_ACTIVATION = 0;
static constexpr uint64_t ACTIVE_FROM_START = 1;


// ---------- Starting the chain and reading blocks from each node ----------

static std::map< std::string, uint64_t > patchMap( uint64_t _sigPatchTimestamp ) {
    return { 
        { "bite2PatchTimestamp", ACTIVE_FROM_START },
        { SIG_PATCH_KEY, _sigPatchTimestamp } 
    };
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

static u256 randomOf( const ConsensusEngine& _engine, node_id _nodeId, uint64_t _blockId ) {
    return ConsensusEngineTestAccess::getNode( _engine, _nodeId )
        ->getSchain()
        ->getRandomForBlockId( block_id( _blockId ) );
}

// Returns the first block with a timestamp at or after _timeStampS, or 0 if there is none.
static uint64_t firstBlockAtOrAfter( const ConsensusEngine& _engine, uint64_t _timeStampS,
    uint64_t _last ) {
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
// every node has committed at least three blocks after that moment. Fails the test unless all
// blocks up to _firstCheckedBlock were committed before the patch turned on.
static ActivationRun runAcrossActivation(
    ConsensusEngine*& _engine, uint64_t _firstCheckedBlock = 1 ) {
    ActivationRun run{};
    run.activationS = activationInSeconds( ACTIVATION_DELAY_S );
    setupFourNodeEngine( _engine, run.activationS );
    // Only read blocks once every node is past the activation time, so the loop does not scan
    // the block database on every poll while the chain is still before it.
    waitOrFail( [&] {
        if ( minLastCommittedTimeStampS( *_engine ) < run.activationS )
            return false;
        run.lastBlock = minLastCommitted( *_engine );
        run.firstActive = firstBlockAtOrAfter( *_engine, run.activationS, run.lastBlock );
        return run.firstActive != 0 && run.firstActive + 2 <= run.lastBlock;
    } );
    CATCH_REQUIRE( run.firstActive > _firstCheckedBlock );
    return run;
}

// Returns the two blocks before the patch turned on and the first three blocks after it,
// skipping any below _minBlockId.
static std::vector< uint64_t > blocksAroundActivation(
    const ActivationRun& _run, uint64_t _minBlockId ) {
    CATCH_REQUIRE( _run.firstActive >= 2 );
    std::vector< uint64_t > result;
    for ( uint64_t blockId = _run.firstActive - 2; blockId <= _run.firstActive + 2; blockId++ ) {
        if ( blockId >= _minBlockId && blockId <= _run.lastBlock )
            result.push_back( blockId );
    }
    return result;
}


// ---------- Expected signature text and checks against it ----------

// A signature is stored as text with four fields: "X:Y:hintY:counter".
using SigTokens = std::array< std::string, 4 >;

// Splits signature text into its four fields. Fails the test if the text does not have exactly
// four non-empty fields.
static SigTokens splitSigString( const std::string& _s ) {
    CATCH_INFO( "Signature string: " << _s );
    CATCH_REQUIRE( std::count( _s.begin(), _s.end(), ':' ) == 3 );
    SigTokens tokens;
    size_t start = 0;
    for ( size_t i = 0; i < tokens.size(); i++ ) {
        auto pos = ( i + 1 < tokens.size() ) ? _s.find( ':', start ) : _s.size();
        tokens[i] = _s.substr( start, pos - start );
        CATCH_REQUIRE( !tokens[i].empty() );
        start = pos + 1;
    }
    return tokens;
}

static std::string joinTokens( const SigTokens& _tokens ) {
    return _tokens[0] + ":" + _tokens[1] + ":" + _tokens[2] + ":" + _tokens[3];
}

// Returns the text every node should store for the signature _stored, which signs _hash.
// The signature point (X and Y) is taken from _stored. The hint is not taken from _stored:
// it is computed again from _hash, because a node may have received it in another format.
static std::string canonicalSigString( const std::string& _stored, const BLAKE3Hash& _hash ) {
    using libBLS::algebra::Base;
    CATCH_INFO( "Stored signature string: " << _stored );
    // Compute the hint the same way a signer does in BLSPrivateKeyShare::sign.
    auto [hashedPoint, hashCounter] = libBLS::algebra::hashToG1withHint( _hash.getHash() );
    const auto hint = hashedPoint.getY().toString( Base::DEC ) + ":" + hashCounter;

    // Let libBLS read X and Y and write the whole signature back out, so the result uses
    // exactly the number format libBLS produces.
    const auto tokens = splitSigString( _stored );
    const auto& x = tokens[0];
    const auto& y = tokens[1];
    return libBLS::BLSSignature( x + ":" + y + ":" + hint, REQUIRED_SIGNERS, TOTAL_SIGNERS )
        .toString();
}

// The hash that the block signature signs.
static BLAKE3Hash blockSigHash( const ptr< CommittedBlock >& _block ) {
    return BLAKE3Hash::getConsensusHash( ( uint64_t ) _block->getProposerIndex(),
        ( uint64_t ) _block->getBlockID(), ( uint64_t ) _block->getSchainID() );
}

// The hash that the reencryption signature signs: the block signature hash followed by the
// reencryption domain text, hashed again.
static BLAKE3Hash reencryptionSigHash( const ptr< CommittedBlock >& _block ) {
    auto baseHash = blockSigHash( _block );
    auto data = std::make_shared< std::vector< uint8_t > >();
    const auto& domain = blockconsensus::REENCRYPTION_RANDOM_DOMAIN;
    data->insert( data->end(), baseHash.getHash().begin(), baseHash.getHash().end() );
    data->insert( data->end(), domain.begin(), domain.end() );
    return BLAKE3Hash::calculateHash( data );
}

static std::string canonicalBlockSig( const ptr< CommittedBlock >& _block ) {
    return canonicalSigString( 
        _block->getThresholdSig(), 
        blockSigHash( _block ) 
    );
}

static std::string canonicalReencryptionSig( const ptr< CommittedBlock >& _block ) {
    auto sig = _block->getReencryptionThresholdSig();
    CATCH_REQUIRE( sig.has_value() );
    return canonicalSigString( *sig, reencryptionSigHash( _block ) );
}

// For every block from _first to _last, checks that each node in _nodes stored the expected
// block signature text and computed the same block random.
static void checkBlockSignaturesOnNodes( const ConsensusEngine& _engine,
    const std::vector< node_id >& _nodes, uint64_t _first, uint64_t _last ) {
    CATCH_REQUIRE( !_nodes.empty() );
    for ( uint64_t blockId = _first; blockId <= _last; blockId++ ) {
        auto reference = blockOf( _engine, _nodes.front(), blockId );
        auto referenceSig = reference->getThresholdSig();
        auto canonicalSig = canonicalBlockSig( reference );
        auto referenceRandom = randomOf( _engine, _nodes.front(), blockId );
        for ( auto nodeId : _nodes ) {
            CATCH_INFO( "block " << blockId << " node " << nodeId << " proposer index "
                                 << ( uint64_t ) reference->getProposerIndex() );
            auto block = blockOf( _engine, nodeId, blockId );
            CATCH_REQUIRE( block->getTimeStampS() == reference->getTimeStampS() );
            CATCH_REQUIRE( block->getThresholdSig() == referenceSig );
            CATCH_REQUIRE( block->getThresholdSig() == canonicalSig );
            CATCH_REQUIRE( randomOf( _engine, nodeId, blockId ) == referenceRandom );
        }
    }
}

// For every block from _first to _last, checks that all nodes stored the expected reencryption
// signature text and computed the same reencryption random.
static void checkReencryptionSignaturesOnAllNodes(
    const ConsensusEngine& _engine, uint64_t _first, uint64_t _last ) {
    for ( uint64_t blockId = _first; blockId <= _last; blockId++ ) {
        auto canonicalSig =
            canonicalReencryptionSig( blockOf( _engine, ALL_NODES.front(), blockId ) );
        auto referenceRandom =
            _engine.getReencryptionRandomForBlockIdForNode( blockId, ALL_NODES.front() );
        CATCH_REQUIRE( referenceRandom != u256( 0 ) );
        for ( auto nodeId : ALL_NODES ) {
            CATCH_INFO( "block " << blockId << " node " << nodeId );
            auto block = blockOf( _engine, nodeId, blockId );
            CATCH_REQUIRE( block->getReencryptionThresholdSig() == canonicalSig );
            CATCH_REQUIRE( _engine.getReencryptionRandomForBlockIdForNode( blockId, nodeId ) ==
                           referenceRandom );
        }
    }
}

// Starts the chain, waits for blocks, and checks the block signature text of every block on
// all nodes.
static void startChainAndCheckBlockSignatures( ConsensusEngine*& _engine, uint64_t _sigPatchTimestamp ) {
    const uint64_t lastBlock = startAndWaitForBlocks( _engine, _sigPatchTimestamp );
    checkBlockSignaturesOnNodes( *_engine, ALL_NODES, 1, lastBlock );
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

// Returns a hook that makes MODIFIED_NODE add a leading zero to the counter of every signature
// share it saves, including its own. If _offchainDomain is true it changes reencryption
// signature shares, otherwise block signature shares. Counts the changed shares in _modified.
// The hook runs on consensus threads, so it only updates the atomic counter.
static std::function< void( node_id, schain_index, bool, std::string& ) >
zeroPadSharesOnModifiedNode( bool _offchainDomain, std::atomic< uint64_t >& _modified ) {
    return [_offchainDomain, &_modified]( node_id _receiverNodeId, schain_index,
               bool _shareOffchainDomain, std::string& _shareString ) {
        if ( _receiverNodeId != MODIFIED_NODE || _shareOffchainDomain != _offchainDomain )
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


// ---------- Catchup test: does a node accept a block whose signature text was changed? ----------

// A named change to a block's signature text. The test puts these signatures into a copy of
// the block and offers it to a node through catchup.
struct SigVariant {
    std::string name;
    std::string thresholdSig;
    std::optional< std::string > reencryptionSig;
};

using SigVariantMaker = std::vector< SigVariant > ( * )( const ptr< CommittedBlock >& );

// Changes to the block signature text. The reencryption signature is left as stored.
// Every change except the last only rewrites the same values in another format.
static std::vector< SigVariant > blockSigVariants( const ptr< CommittedBlock >& _block ) {
    const auto [x, y, hintY, counter] = splitSigString( _block->getThresholdSig() );
    const auto otherCounter = std::to_string( std::stoull( counter ) + 1 );

    auto reencryptionSig = _block->getReencryptionThresholdSig();
    auto variant = [&]( const std::string& _name, const std::string& _thresholdSig ) {
        return SigVariant{ _name, _thresholdSig, reencryptionSig };
    };

    return {
        variant( "zero-padded X", joinTokens( { "0" + x, y, hintY, counter } ) ),
        variant( "zero-padded Y", joinTokens( { x, "0" + y, hintY, counter } ) ),
        variant( "zero-padded hint", joinTokens( { x, y, "0" + hintY, counter } ) ),
        variant( "zero-padded counter", joinTokens( { x, y, hintY, "0" + counter } ) ),
        variant( "empty token", x + "::" + y + ":" + hintY + ":" + counter ),
        variant( "other counter", joinTokens( { x, y, hintY, otherCounter } ) ),
    };
}

// Changes to the reencryption signature text. The block signature is left as stored.
static std::vector< SigVariant > reencryptionSigVariants( const ptr< CommittedBlock >& _block ) {
    const auto reencryptionSig = _block->getReencryptionThresholdSig();
    CATCH_REQUIRE( reencryptionSig.has_value() );
    const auto [x, y, hintY, counter] = splitSigString( *reencryptionSig );

    const auto blockSig = _block->getThresholdSig();
    auto variant = [&]( const std::string& _name, const std::string& _reencryptionSig ) {
        return SigVariant{ _name, blockSig, _reencryptionSig };
    };

    return {
        // A valid signature, but of the wrong hash: the block hash, not the reencryption hash.
        variant( "block signature as reencryption signature", blockSig ),
        variant( "zero-padded counter", joinTokens( { x, y, hintY, "0" + counter } ) ),
    };
}

// Returns a copy of _block with the given signature texts.
static ptr< CommittedBlock > rebuildBlock( const ptr< CommittedBlock >& _block,
    const std::string& _thresholdSig, const std::optional< std::string >& _reencryptionSig,
    const ptr< DecryptedAESKeyList >& _aesKeys ) {
    return CommittedBlock::make( _block->getSchainID(), _block->getProposerNodeID(),
        _block->getBlockID(), _block->getEpochID(), _block->getProposerIndex(),
        _block->getTransactionList(), _block->getStateRoot(), _block->getTimeStampS(),
        _block->getTimeStampMs(), _block->getSignature(), _thresholdSig, _reencryptionSig,
        _block->getDaSig(), _aesKeys, _block->getDecryptedTransactions() );
}

// Takes block _blockId from node 1, makes one copy per change from _makeVariants, and reads
// each copy the way node 1 reads a block received through catchup (with signature checks).
// If the block was committed after the patch turned on, every changed copy must be rejected.
// Otherwise every changed copy must be accepted, as before the patch.
static void checkCatchupOfChangedSignatures( const ConsensusEngine& _engine, uint64_t _blockId,
    uint64_t _activationS, SigVariantMaker _makeVariants ) {
    auto node = ConsensusEngineTestAccess::getNode( _engine, Chain::NODE_1 );
    auto cryptoManager = node->getSchain()->getCryptoManager();
    auto biteManager = node->getSchain()->getBiteManager();
    CATCH_REQUIRE( cryptoManager );
    CATCH_REQUIRE( biteManager );

    auto block = blockOf( _engine, Chain::NODE_1, _blockId );
    auto storedKeys = block->getDecryptedAesKeyList();
    CATCH_REQUIRE( storedKeys );
    // CommittedBlock::make takes a non-const key list, so the copies get their own copy of it.
    auto aesKeys = std::make_shared< DecryptedAESKeyList >( *storedKeys );
    const bool mustBeRejected =
        _activationS != NO_ACTIVATION && block->getTimeStampS() >= _activationS;

    // Builds a copy of the block with the given signatures and reads it as a catchup block.
    // Returns the error message if the copy is rejected, or "" if it is accepted.
    auto catchupError = [&]( const std::string& _thresholdSig,
                         const std::optional< std::string >& _reencryptionSig ) -> std::string {
        auto rebuilt = rebuildBlock( block, _thresholdSig, _reencryptionSig, aesKeys );
        auto serialized = rebuilt->serialize();
        try {
            CommittedBlock::deserialize( serialized, cryptoManager, biteManager, true );
            return "";
        } catch ( const std::exception& e ) {
            return e.what();
        } catch ( ... ) {
            return "unknown exception";
        }
    };

    CATCH_INFO( "block " << _blockId << " timestamp " << block->getTimeStampS()
                         << " activation " << _activationS );

    // An unchanged copy must always be accepted.
    CATCH_REQUIRE(
        catchupError( block->getThresholdSig(), block->getReencryptionThresholdSig() ) == "" );

    for ( const auto& variant : _makeVariants( block ) ) {
        CATCH_INFO( "variant: " << variant.name << ", threshold signature "
                                << variant.thresholdSig );
        const auto error = catchupError( variant.thresholdSig, variant.reencryptionSig );
        CATCH_INFO( "catchup error: " << ( error.empty() ? "none" : error ) );
        const bool rejected = !error.empty();
        CATCH_REQUIRE( rejected == mustBeRejected );
    }
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

    std::atomic< uint64_t > modifiedShares{ 0 };
    std::optional< TestHooks::BlockSigShareStoreHook::Scope > modifyHintScope;
    std::optional< TestHooks::BlockCatchupResponseHook::Scope > catchupScope;

    CATCH_SECTION( "activation from the start, modified share encoding on one node" ) {
        modifyHintScope.emplace( zeroPadSharesOnModifiedNode( false, modifiedShares ) );
        catchupScope.emplace( isolateModifiedNodeCatchup() );

        startChainAndCheckBlockSignatures( testEngine, ACTIVE_FROM_START );
        CATCH_REQUIRE( modifiedShares.load() > 0 );
    }

    CATCH_SECTION( "unmodified run keeps the canonical bytes" ) {
        startChainAndCheckBlockSignatures( testEngine, ACTIVE_FROM_START );
    }

    CATCH_SECTION( "unmodified run without activation keeps the canonical bytes" ) {
        startChainAndCheckBlockSignatures( testEngine, NO_ACTIVATION );
    }

    CATCH_SECTION( "activation in the future, modified share encoding on one node" ) {
        modifyHintScope.emplace( zeroPadSharesOnModifiedNode( false, modifiedShares ) );
        catchupScope.emplace( isolateModifiedNodeCatchup() );

        const auto run = runAcrossActivation( testEngine );
        CATCH_REQUIRE( modifiedShares.load() > 0 );

        // Before the patch turns on, the other nodes store the expected text, but MODIFIED_NODE
        // stores the format it received. Check this for the last block before the patch.
        checkBlockSignaturesOnNodes( *testEngine, UNCHANGED_NODES, 1, run.firstActive - 1 );
        CATCH_REQUIRE(
            blockOf( *testEngine, MODIFIED_NODE, run.firstActive - 1 )->getThresholdSig() !=
            blockOf( *testEngine, Chain::NODE_1, run.firstActive - 1 )->getThresholdSig() );

        // After the patch turns on, all nodes, MODIFIED_NODE included, store the expected text.
        checkBlockSignaturesOnNodes( *testEngine, ALL_NODES, run.firstActive, run.lastBlock );
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
        const auto run = runAcrossActivation( testEngine );

        for ( auto blockId : blocksAroundActivation( run, 1 ) )
            checkCatchupOfChangedSignatures( *testEngine, blockId, run.activationS, blockSigVariants );
    }

    CATCH_SECTION( "no activation" ) {
        const uint64_t lastBlock = startAndWaitForBlocks( testEngine, NO_ACTIVATION );

        const uint64_t lastChecked = std::min( lastBlock, CATCHUP_CHECKED_BLOCKS );
        for ( uint64_t blockId = 1; blockId <= lastChecked; blockId++ )
            checkCatchupOfChangedSignatures( *testEngine, blockId, NO_ACTIVATION, blockSigVariants );
    }

    CATCH_SUCCEED();
}

// The same checks as above, for the reencryption signature and the reencryption random.
CATCH_TEST_CASE( "Reencryption signature string is canonical and verified",
    "[bite][sgx][block-signature][reencryption][end-to-end]" ) {

    E2EHelper::configureTestEnvironment( true, Chain::CONFIG_DIR );

    ConsensusEngine* testEngine = nullptr;
    E2ETestUtils::ScopedEngineShutdown engineShutdown( testEngine );

    std::atomic< uint64_t > modifiedShares{ 0 };

    CATCH_SECTION( "reencryption random is identical on all nodes" ) {
        TestHooks::BlockSigShareStoreHook::Scope modifyHintScope(
            zeroPadSharesOnModifiedNode( true, modifiedShares ) );
        TestHooks::BlockCatchupResponseHook::Scope catchupScope( isolateModifiedNodeCatchup() );

        const uint64_t lastBlock = startAndWaitForBlocks( testEngine, ACTIVE_FROM_START );
        CATCH_REQUIRE( modifiedShares.load() > 0 );

        checkReencryptionSignaturesOnAllNodes(
            *testEngine, FIRST_REENCRYPTION_BLOCK_ID, lastBlock );
    }

    CATCH_SECTION( "reencryption signature on catchup" ) {
        const auto run = runAcrossActivation( testEngine, FIRST_REENCRYPTION_BLOCK_ID );

        for ( auto blockId : blocksAroundActivation( run, FIRST_REENCRYPTION_BLOCK_ID ) )
            checkCatchupOfChangedSignatures(
                *testEngine, blockId, run.activationS, reencryptionSigVariants );
    }

    CATCH_SUCCEED();
}

#endif  // BITE
