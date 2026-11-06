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
#include "node/ConsensusEngine.h"
#include "utils/Time.h"
#include "tests/e2e/ConsensusEngineTestAccess.h"
#include "tests/e2e/E2ETestHelper.h"
#include "tests/TestUtils.h"

#include <boost/filesystem.hpp>

#include <atomic>
#include <functional>

namespace EpochIdUpdateTests {

using E2EHelper = E2ETestUtils::E2ETestHelper;

static constexpr uint64_t BOUNDARY_DELAY_S = 5;
static constexpr uint64_t BLOCKS_AFTER_UPDATE = 5;
static constexpr uint64_t WAIT_TIMEOUT_MS = 90000;

// Passes the next epoch id to the engine once, after the first committed block reaches the
// boundary timestamp, like skaled does after importing that block. All nodes of this test
// run in one engine, so this covers the engine passing the epoch id to its nodes and blocks
// being produced after the change.
class EpochUpdatingExtFace : public ConsensusExtFace {
    std::atomic< ConsensusEngine* > engine{ nullptr };
    std::atomic< uint64_t > boundaryTimestampS{ uint64_t( -1 ) };
    std::atomic< uint64_t > firstBlockAfterBoundary{ 0 };

public:
    void setEngine( ConsensusEngine* _engine ) { engine = _engine; }

    void setBoundaryTimestampS( uint64_t _timestampS ) { boundaryTimestampS = _timestampS; }

    uint64_t getFirstBlockAfterBoundary() const { return firstBlockAfterBoundary; }

    Transactions pendingTransactions( size_t, u256& _stateRoot ) override {
        _stateRoot = 0;
        return Transactions();
    }

    void createBlock( const Transactions&, DecryptedTransactions, uint64_t _timeStamp, uint32_t,
        uint64_t _blockID, u256, u256, uint64_t ) override {
        auto consensusEngine = engine.load();
        if ( consensusEngine == nullptr || _timeStamp < boundaryTimestampS )
            return;

        uint64_t expected = 0;
        if ( firstBlockAfterBoundary.compare_exchange_strong( expected, _blockID ) )
            consensusEngine->setEpochId( 1 );
    }
};

// Config dir copy without ../../run_sgx_test next to it, so the nodes use mockup signatures
// (test configs switch to SGX when run_sgx_test/sgx_data exists)
string copyConfigForMockup( const string& _configDir ) {
    auto from = boost::filesystem::system_complete( _configDir );
    auto root = boost::filesystem::path( "/tmp/consensus_epoch_id_test_config" );
    auto to = root / "test" / from.filename();
    boost::filesystem::remove_all( root );
    boost::filesystem::create_directories( to );
    for ( auto& entry : boost::filesystem::recursive_directory_iterator( from ) ) {
        auto target = to / boost::filesystem::relative( entry.path(), from );
        if ( boost::filesystem::is_directory( entry.path() ) )
            boost::filesystem::create_directories( target );
        else
            boost::filesystem::copy_file( entry.path(), target );
    }
    return to.string();
}

bool waitFor( const std::function< bool() >& _condition, uint64_t _timeoutMs ) {
    auto deadline = Time::getCurrentTimeMs() + _timeoutMs;
    while ( Time::getCurrentTimeMs() < deadline ) {
        if ( _condition() )
            return true;
        usleep( 200 * 1000 );
    }
    return _condition();
}

}  // namespace EpochIdUpdateTests

using namespace EpochIdUpdateTests;

CATCH_TEST_CASE( "epoch id passed by the host after startup reaches every node",
    "[epoch][end-to-end][db][bite]" ) {
    ConsensusEngine* testEngine = nullptr;
    EpochUpdatingExtFace extFace;

    try {
        E2EHelper::configureTestEnvironment( true, copyConfigForMockup( "test/fournodes" ) );

        E2EHelper::startEngine( testEngine, 0,
            {
                { "bite2PatchTimestamp", 0 },
            },
            &extFace );
        extFace.setEngine( testEngine );
        extFace.setBoundaryTimestampS( Time::getCurrentTimeSec() + BOUNDARY_DELAY_S );

        auto nodeIds = testEngine->getNodeIDs();
        CATCH_REQUIRE( nodeIds.size() == 4 );

        for ( auto nodeId : nodeIds ) {
            CATCH_REQUIRE(
                ConsensusEngineTestAccess::getCurrentEpochIdForNode( *testEngine, nodeId ) == 0 );
        }

        // every node must keep committing blocks after the epoch update
        CATCH_REQUIRE( waitFor(
            [&]() {
                auto first = extFace.getFirstBlockAfterBoundary();
                if ( first == 0 )
                    return false;
                for ( auto nodeId : nodeIds ) {
                    auto committed = (uint64_t) ConsensusEngineTestAccess::
                        getCurrentLastCommittedBlockIDForNode( *testEngine, nodeId );
                    if ( committed < first + BLOCKS_AFTER_UPDATE )
                        return false;
                }
                return true;
            },
            WAIT_TIMEOUT_MS ) );

        for ( auto nodeId : nodeIds ) {
            CATCH_CHECK(
                ConsensusEngineTestAccess::getCurrentEpochIdForNode( *testEngine, nodeId ) == 1 );
        }

        extFace.setEngine( nullptr );
        E2EHelper::stopEngineGracefully( testEngine );
    } catch ( ... ) {
        extFace.setEngine( nullptr );
        if ( testEngine ) {
            E2EHelper::stopEngineGracefully( testEngine );
        }
        throw;
    }
}

#endif  // BITE
