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
#include "Consensust.h"
#include "SkaleCommon.h"
#include "datastructures/CommittedBlock.h"
#include "datastructures/TransactionList.h"
#include "tests/e2e/ConsensusEngineTestAccess.h"
#include "tests/e2e/E2ETestHelper.h"
#include "utils/Time.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <vector>

namespace RestartBootstrapTests {

using E2EHelper = E2ETestUtils::E2ETestHelper;

static constexpr uint64_t PATCH_DELAY_MS = 4000;
static constexpr uint64_t SNAPSHOT_WAIT_TIMEOUT_MS = 20000;
static constexpr uint64_t SNAPSHOT_POLL_INTERVAL_MS = 250;
static constexpr uint64_t POST_PATCH_SETTLE_MS = 1500;
static constexpr uint64_t REPLAY_WAIT_TIMEOUT_MS = 10000;

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
    size_t expectedReplayStartIndex = 0;
};

// ConsensusExtFace implementation that records blocks created
// by the engine and allows waiting for a certain number of blocks to be recorded.
class RecordingConsensusExtFace : public ConsensusExtFace {
    mutable std::mutex mutex;
    std::condition_variable condition;
    std::vector< BlockSnapshot > replayedBlocks;

public:
    Transactions pendingTransactions( size_t, u256& _stateRoot ) override {
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

bool hasBothSidesOfTransition( const std::vector< BlockSnapshot >& _snapshots ) {
#ifdef BITE
    bool hasBeforePatch = false;
    bool hasAfterPatch = false;

    for ( const auto& snapshot : _snapshots ) {
        hasBeforePatch = hasBeforePatch || !snapshot.isBite2Enabled;
        hasAfterPatch = hasAfterPatch || snapshot.isBite2Enabled;
    }

    return hasBeforePatch && hasAfterPatch;
#else
    (void) _snapshots;
    return true;
#endif
}

std::vector< BlockSnapshot > waitForCommittedSnapshots(
    ConsensusEngine& _engine, uint64_t _minWaitMs ) {
    auto startTimeMs = Time::getCurrentTimeMs();
    auto deadlineMs = startTimeMs + SNAPSHOT_WAIT_TIMEOUT_MS;

    // wait for a max time
    while ( Time::getCurrentTimeMs() < deadlineMs ) {
        auto lastId = (uint64_t) _engine.getLargestCommittedBlockIDInDb();
        auto elapsedMs = Time::getCurrentTimeMs() - startTimeMs;

        // if passed minimum wait time
        if ( elapsedMs >= _minWaitMs && lastId > 0 ) {
            auto snapshots = buildSnapshots( _engine, lastId );
#ifdef BITE
            // if we have at least 1 BITE block - return early
            if ( hasBothSidesOfTransition( snapshots ) ) {
                return snapshots;
            }
#else
            return snapshots;
#endif
        }

        usleep( SNAPSHOT_POLL_INTERVAL_MS * 1000 );
    }

    CATCH_FAIL( "Timed out waiting for committed blocks to stabilize before restart" );
    return {};
}

void runReplayScenario(
    const ReplayStartState& _startState,
    const std::map< string, uint64_t >& _patchTimestamps ) {
    CATCH_CAPTURE( _startState.name );
    CATCH_CAPTURE( _startState.lastCommittedBlockId );
    CATCH_CAPTURE( _startState.lastCommittedBlockTimestampS );
    CATCH_CAPTURE( _startState.lastCommittedBlockTimestampMs );
    CATCH_CAPTURE( _startState.expectedReplayStartIndex );

    ConsensusEngine* replayEngine = nullptr;

    try {
        RecordingConsensusExtFace extFace;
        E2EHelper::startEngine( replayEngine, (int64_t) _startState.lastCommittedBlockId,
            _patchTimestamps, &extFace, _startState.lastCommittedBlockTimestampS,
            _startState.lastCommittedBlockTimestampMs, false );

        auto bootstrapBlockID =
            (uint64_t) ConsensusEngineTestAccess::getBootstrapBlockID( *replayEngine );
        auto authoritativeSnapshots = buildSnapshots( *replayEngine, bootstrapBlockID );
        auto expectedReplaySnapshots = std::vector< BlockSnapshot >(
            authoritativeSnapshots.begin() + _startState.expectedReplayStartIndex,
            authoritativeSnapshots.end() );

        CATCH_REQUIRE( !expectedReplaySnapshots.empty() );
        CATCH_REQUIRE( bootstrapBlockID == authoritativeSnapshots.back().blockId );

        auto actualSnapshots = extFace.waitForAtLeast( expectedReplaySnapshots.size() );
        CATCH_REQUIRE( actualSnapshots.size() >= expectedReplaySnapshots.size() );
        actualSnapshots.resize( expectedReplaySnapshots.size() );

#ifdef BITE
        fillBiteFieldsFromEngine( *replayEngine, actualSnapshots );
#endif

        for ( size_t i = 0; i < expectedReplaySnapshots.size(); ++i ) {
            CATCH_CAPTURE( i );
            const auto& expected = expectedReplaySnapshots.at( i );
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
        auto replayTip = expectedReplaySnapshots.back();

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

    try {
        E2EHelper::configureTestEnvironment( true );

        auto bite2PatchTimestamp = ( Time::getCurrentTimeMs() + PATCH_DELAY_MS + 999 ) / 1000;
        patchTimestamps = {
#ifdef BITE
            { "bite2PatchTimestamp", bite2PatchTimestamp }
#endif
        };


        // ------- 1st Run to produce some blocks -------

        E2EHelper::startEngine( initialEngine, 0, patchTimestamps );

        // wait either for minimum waittime OR until we have at least 1 bite2 block committed.
        snapshots = waitForCommittedSnapshots(
            *initialEngine, PATCH_DELAY_MS + POST_PATCH_SETTLE_MS );

        CATCH_REQUIRE( !snapshots.empty() );
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
        { "replay-from-zero", 0, 0, 0, 0 }, patchTimestamps );
    runReplayScenario(
        { "replay-from-midpoint", midpointSnapshot.blockId, midpointSnapshot.timestampS,
            midpointSnapshot.timestampMs, midpointIndex },
        patchTimestamps );

    CATCH_SUCCEED();
}
