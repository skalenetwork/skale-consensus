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

#include "thirdparty/catch.hpp"
#include "SkaleCommon.h"
#include "datastructures/CommittedBlock.h"
#include "datastructures/TransactionList.h"
#include "pendingqueue/TestMessageGeneratorAgent.h"
#include "tests/e2e/ConsensusEngineTestAccess.h"
#include "tests/e2e/E2ETestHelper.h"
#include "utils/Time.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace RestartBootstrapTests {

using E2EHelper = E2ETestUtils::E2ETestHelper;

static constexpr uint64_t PATCH_DELAY_MS = 4000;
static constexpr uint64_t SNAPSHOT_WAIT_TIMEOUT_MS = 60000;
static constexpr uint64_t POST_PATCH_SETTLE_MS = 1500;
static constexpr uint64_t REPLAY_WAIT_TIMEOUT_MS = 10000;
static constexpr uint64_t MAX_PRE_RESTART_BLOCKS = 10;

struct BlockSnapshot {
    uint64_t blockId = 0;
    uint64_t timestampS = 0;
    uint32_t timestampMs = 0;
    uint64_t proposerIndex = 0;
    size_t txCount = 0;
    u256 stateRoot = 0;
#ifdef BITE
    bool isBite2Enabled = false;
    std::optional< u256 > reencryptionRandom;
#endif
};

struct ReplayStartState {
    const char* name = "";
    uint64_t lastCommittedBlockId = 0;
    uint64_t lastCommittedBlockTimestampS = 0;
    uint64_t lastCommittedBlockTimestampMs = 0;
};

ConsensusExtFace::Transactions generateTestTransactions(
    ConsensusEngine& _engine, size_t _limit, u256& _stateRoot ) {
    auto node = ConsensusEngineTestAccess::getNode(
        _engine, ConsensusEngineTestAccess::getFirstNodeId( _engine ) );
    auto generator = node->getSchain()->getTestMessageGeneratorAgent();
    _stateRoot = 7;
#ifdef BITE
    return generator->pendingTransactionsBITE( _limit );
#else
    return generator->pendingTransactions( _limit );
#endif
}

// ConsensusExtFace implementation that records blocks created
// by the engine and allows waiting for a certain number of blocks to be recorded.
class RecordingConsensusExtFace : public ConsensusExtFace {
    std::mutex mutex;
    std::condition_variable condition;
    std::vector< BlockSnapshot > replayedBlocks;

    // Borrowed; configured before startup and kept alive until all workers stop.
    ConsensusEngine* initialEngine = nullptr;
    uint64_t bite2PatchTimestamp = 0;

    void controlInitialRunAfterBlock( uint64_t _blockID ) {
        CHECK_STATE( initialEngine );
#ifdef BITE
        if ( _blockID == 1 ) {
            // Keep the pre-patch block without producing more blocks while waiting.
            std::this_thread::sleep_until( std::chrono::system_clock::time_point(
                std::chrono::milliseconds(
                    bite2PatchTimestamp * 1000 + POST_PATCH_SETTLE_MS ) ) );
        }
#endif
        if ( _blockID == MAX_PRE_RESTART_BLOCKS ) {
            initialEngine->exitGracefully();
        }
    }

public:
    RecordingConsensusExtFace() = default;

    void configureInitialRun( ConsensusEngine& _engine, uint64_t _bite2PatchTimestamp ) {
        CHECK_STATE( initialEngine == nullptr );
        initialEngine = &_engine;
        bite2PatchTimestamp = _bite2PatchTimestamp;
    }

    Transactions pendingTransactions( size_t _limit, u256& _stateRoot ) override {
        if ( initialEngine ) {
            return generateTestTransactions( *initialEngine, _limit, _stateRoot );
        }
        _stateRoot = 0;
        return Transactions();
    }

    void createBlock( const Transactions& _approvedTransactions,
#ifdef BITE
        DecryptedTransactions,
#endif
        uint64_t _timeStamp, uint32_t _timeStampMillis, uint64_t _blockID, u256, u256 _stateRoot,
        uint64_t _winningNodeIndex ) override {
        {
            std::lock_guard< std::mutex > lock( mutex );
            BlockSnapshot snapshot;
            snapshot.blockId = _blockID;
            snapshot.timestampS = _timeStamp;
            snapshot.timestampMs = _timeStampMillis;
            snapshot.proposerIndex = _winningNodeIndex;
            snapshot.txCount = _approvedTransactions.size();
            snapshot.stateRoot = _stateRoot;
            replayedBlocks.push_back( snapshot );
        }
        if ( initialEngine ) {
            controlInitialRunAfterBlock( _blockID );
        }

        condition.notify_all();
    }

    std::vector< BlockSnapshot > waitForAtLeast(
        size_t _expectedCount, uint64_t _timeoutMs = REPLAY_WAIT_TIMEOUT_MS ) {
        std::unique_lock< std::mutex > lock( mutex );
        auto completed = condition.wait_for( lock, std::chrono::milliseconds( _timeoutMs ),
            [this, _expectedCount]() { return replayedBlocks.size() >= _expectedCount; } );
        CATCH_REQUIRE( completed );
        return replayedBlocks;
    }
};

#ifdef BITE
void fillBiteFieldsFromEngine(
    ConsensusEngine& _engine, std::vector< BlockSnapshot >& _snapshots ) {
    for ( auto& snapshot : _snapshots ) {
        try {
            snapshot.reencryptionRandom =
                _engine.getReencryptionRandomForBlockId( snapshot.blockId );
            snapshot.isBite2Enabled = true;
        } catch ( ... ) {
            snapshot.reencryptionRandom = std::nullopt;
            snapshot.isBite2Enabled = false;
        }
    }
}
#endif

std::vector< BlockSnapshot > buildSnapshots(
    ConsensusEngine& _engine, uint64_t _lastId ) {
    std::vector< BlockSnapshot > snapshots;

    for ( uint64_t blockId = 1; blockId <= _lastId; ++blockId ) {
        auto block = ConsensusEngineTestAccess::getCommittedBlockForBlockId( _engine, blockId );
        CATCH_REQUIRE( block != nullptr );

        // fill normal fields here directly
        BlockSnapshot snapshot;
        snapshot.blockId = blockId;
        snapshot.timestampS = block->getTimeStampS();
        snapshot.timestampMs = block->getTimeStampMs();
        snapshot.proposerIndex = (uint64_t) block->getProposerIndex();
        snapshot.txCount = (size_t) block->getTransactionList()->size();
        snapshot.stateRoot = block->getStateRoot();

        snapshots.push_back( snapshot );
    }

#ifdef BITE
    // fill bite fields for all blocks
    fillBiteFieldsFromEngine( _engine, snapshots );
#endif

    return snapshots;
}

#ifdef BITE
bool hasBothSidesOfTransition( const std::vector< BlockSnapshot >& _snapshots ) {
    bool hasBeforePatch = false;
    bool hasAfterPatch = false;

    for ( const auto& snapshot : _snapshots ) {
        hasBeforePatch = hasBeforePatch || !snapshot.isBite2Enabled;
        hasAfterPatch = hasAfterPatch || snapshot.isBite2Enabled;
    }

    return hasBeforePatch && hasAfterPatch;
}
#endif

void runReplayScenario(
    const ReplayStartState& _startState,
    const std::map< string, uint64_t >& _patchTimestamps,
    const std::vector< BlockSnapshot >& _originalSnapshots ) {
    CATCH_CAPTURE( _startState.name );
    CATCH_CAPTURE( _startState.lastCommittedBlockId );
    CATCH_CAPTURE( _startState.lastCommittedBlockTimestampS );
    CATCH_CAPTURE( _startState.lastCommittedBlockTimestampMs );

    CATCH_REQUIRE( _startState.lastCommittedBlockId < _originalSnapshots.size() );
    const auto replayStartIndex = static_cast< size_t >( _startState.lastCommittedBlockId );
    const auto expectedReplayCount = _originalSnapshots.size() - replayStartIndex;

    ConsensusEngine* replayEngine = nullptr;
    RecordingConsensusExtFace extFace;

    try {
        E2EHelper::startEngine( replayEngine, (int64_t) _startState.lastCommittedBlockId,
            _patchTimestamps, &extFace, _startState.lastCommittedBlockTimestampS,
            _startState.lastCommittedBlockTimestampMs, false );

        auto bootstrapBlockID =
            (uint64_t) ConsensusEngineTestAccess::getBootstrapBlockID( *replayEngine );
        CATCH_REQUIRE( bootstrapBlockID >= _originalSnapshots.back().blockId );

        auto actualSnapshots = extFace.waitForAtLeast( expectedReplayCount );
        actualSnapshots.resize( expectedReplayCount );

#ifdef BITE
        fillBiteFieldsFromEngine( *replayEngine, actualSnapshots );
#endif

        for ( size_t i = 0; i < expectedReplayCount; ++i ) {
            CATCH_CAPTURE( i );
            const auto& expected = _originalSnapshots.at( replayStartIndex + i );
            const auto& actual = actualSnapshots.at( i );

            CATCH_REQUIRE( actual.blockId == expected.blockId );
            CATCH_REQUIRE( actual.timestampS == expected.timestampS );
            CATCH_REQUIRE( actual.timestampMs == expected.timestampMs );
            CATCH_REQUIRE( actual.proposerIndex == expected.proposerIndex );
            CATCH_REQUIRE( actual.txCount == expected.txCount );
            CATCH_REQUIRE( actual.stateRoot == expected.stateRoot );

#ifdef BITE
            CATCH_REQUIRE( actual.isBite2Enabled == expected.isBite2Enabled );
            CATCH_REQUIRE( actual.reencryptionRandom == expected.reencryptionRandom );
#endif
        }

        auto currentLastCommittedBlockID =
            (uint64_t) ConsensusEngineTestAccess::getCurrentLastCommittedBlockID( *replayEngine );
        auto currentLastCommittedBlockTimeStamp =
            ConsensusEngineTestAccess::getCurrentLastCommittedBlockTimeStamp( *replayEngine );
        const auto& replayTip = _originalSnapshots.back();

        CATCH_REQUIRE( currentLastCommittedBlockID >= bootstrapBlockID );
        if ( currentLastCommittedBlockID == replayTip.blockId ) {
            CATCH_REQUIRE( currentLastCommittedBlockTimeStamp.getS() == replayTip.timestampS );
            CATCH_REQUIRE( currentLastCommittedBlockTimeStamp.getMs() == replayTip.timestampMs );
        } else {
            CATCH_REQUIRE( currentLastCommittedBlockTimeStamp.getS() >= replayTip.timestampS );
            if ( currentLastCommittedBlockTimeStamp.getS() == replayTip.timestampS ) {
                CATCH_REQUIRE( currentLastCommittedBlockTimeStamp.getMs() >= replayTip.timestampMs );
            }
        }

        E2EHelper::stopEngineGracefully( replayEngine );
    } catch ( ... ) {
        if ( replayEngine ) {
            E2EHelper::stopEngineGracefully( replayEngine );
        }
        throw;
    }
}

}  // namespace RestartBootstrapTests

using namespace RestartBootstrapTests;

CATCH_TEST_CASE(
    "restart bootstrap replays committed blocks",
    "[restart-bootstrap][end-to-end][db]" ) {
    ConsensusEngine* initialEngine = nullptr;
    std::vector< BlockSnapshot > snapshots;
    std::map< string, uint64_t > patchTimestamps;
    auto bite2PatchTimestamp = ( Time::getCurrentTimeMs() + PATCH_DELAY_MS + 999 ) / 1000;


    RecordingConsensusExtFace initialExtFace;

    try {
        E2EHelper::configureTestEnvironment( true );

        patchTimestamps = {
#ifdef BITE
            { "bite2PatchTimestamp", bite2PatchTimestamp }
#endif
        };


        // ------- 1st Run to produce some blocks -------

        E2EHelper::setupEngine( initialEngine, 0, patchTimestamps, &initialExtFace );
        initialExtFace.configureInitialRun( *initialEngine, bite2PatchTimestamp );
        initialEngine->slowStartBootStrapTest();

        initialExtFace.waitForAtLeast( MAX_PRE_RESTART_BLOCKS, SNAPSHOT_WAIT_TIMEOUT_MS );
        initialEngine->testExitGracefullyBlocking();

        auto lastId = (uint64_t) initialEngine->getLargestCommittedBlockIDInDb();
        CATCH_REQUIRE( lastId == MAX_PRE_RESTART_BLOCKS );
        snapshots = buildSnapshots( *initialEngine, lastId );

        CATCH_REQUIRE( !snapshots.empty() );
        for ( const auto& snapshot : snapshots ) {
            CATCH_CAPTURE( snapshot.blockId );
            CATCH_REQUIRE( snapshot.txCount > 0 );
        }
#ifdef BITE
        CATCH_REQUIRE( hasBothSidesOfTransition( snapshots ) );
#endif

        E2EHelper::stopEngineGracefully( initialEngine );
    } catch ( SkaleException& e ) {
        if ( initialEngine ) {
            E2EHelper::stopEngineGracefully( initialEngine );
        }
        SkaleException::logNested( e );
        throw;
    } catch ( ... ) {
        if ( initialEngine ) {
            E2EHelper::stopEngineGracefully( initialEngine );
        }
        throw;
    }

    auto midpointIndex = snapshots.size() / 2;
    CATCH_REQUIRE( midpointIndex > 0 );
    CATCH_REQUIRE( midpointIndex < snapshots.size() );
    const auto& midpointSnapshot = snapshots.at( midpointIndex - 1 );

    runReplayScenario(
        { "replay-from-zero", 0, 0, 0 }, patchTimestamps, snapshots );
    runReplayScenario(
        { "replay-from-midpoint", midpointSnapshot.blockId, midpointSnapshot.timestampS,
            midpointSnapshot.timestampMs },
        patchTimestamps, snapshots );
}
