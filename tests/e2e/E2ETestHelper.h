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

#pragma once

#include "thirdparty/catch.hpp"
#include "Consensust.h"
#include "SkaleCommon.h"
#include "node/ConsensusEngine.h"
#include "node/Node.h"
#include "tests/TestUtils.h"
#include "tests/e2e/ConsensusEngineTestAccess.h"
#include "tests/harness/consensus/ConsensusStartup.h"
#include "tests/harness/transport/SocketIO.h"

#include <boost/filesystem.hpp>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace E2ETestUtils {

// Arguments of E2ETestHelper::setupEngine and startEngine. Every field has a default, so a
// test names only the fields it changes, in the order they are declared here.
struct EngineOptions {
    int64_t lastCommittedBlockId = 0;
    std::map< string, uint64_t > patchTimestamps = ConsensusStartupOptions{}.patchTimestamps;
    // Borrowed; must outlive the engine. Requires lastCommittedBlockId >= 0.
    ConsensusExtFace* extFace = nullptr;
    uint64_t lastCommittedTimestampS = 0;
    uint64_t lastCommittedTimestampMs = 0;
    bool useBlockIdFromConsensus = false;
    // Fail the test unless every node signs with SGX keys. When run_sgx_test/sgx_data is
    // missing, the nodes silently fall back to mock crypto, which accepts any signature or
    // share, so a test of a cryptographic check would pass without checking anything.
    bool requireSgx = false;
};

class E2ETestHelper {
public:
    inline static const string TEST_DATA_ROOT_DIR = "/tmp/consensus_tests";
    inline static const string ONE_NODE_TEST_DATA_DIR = TEST_DATA_ROOT_DIR + "/onenode";
    inline static const string TWO_NODE_TEST_DATA_DIR = TEST_DATA_ROOT_DIR + "/twonodes_sameip";
    static constexpr uint64_t DEFAULT_RUN_TIME_S = 4;
    static constexpr uint64_t CROSS_NODE_RUN_TIME_S = 20;
    static constexpr uint64_t DEFAULT_STORAGE_LIMIT_BYTES =
        ConsensusStartupOptions::DEFAULT_STORAGE_LIMIT_BYTES;

    static void configureTestEnvironment(
        bool _cleanDataDir,
        const string& _configDir = "",
        const string& _restartSnapshotFile = "/tmp/consensus_pre_restart.txt" ) {
        auto cfgDir =
            boost::filesystem::system_complete( _configDir.empty() ? "test/onenode" : _configDir );
        CATCH_REQUIRE( boost::filesystem::exists( cfgDir ) );
        CATCH_REQUIRE( boost::filesystem::is_directory( cfgDir ) );
        Consensust::setConfigDirPath( cfgDir );

        if ( _cleanDataDir ) {
            boost::filesystem::remove_all( TEST_DATA_ROOT_DIR );
            if ( !_restartSnapshotFile.empty() ) {
                std::remove( _restartSnapshotFile.c_str() );
            }
        }

        boost::filesystem::create_directories( TEST_DATA_ROOT_DIR );

        setTestEnvVar( "DATA_DIR", TEST_DATA_ROOT_DIR.c_str() );

        // Keep default runtime for fast tests unless caller already set TEST_TIME_S.
        setTestEnvVar( "TEST_TIME_S", "4", 0 );
        // Functional tests use small blocks unless the caller requests a larger workload.
        setTestEnvVar( "TEST_TRANSACTIONS_PER_BLOCK", "10", 0 );
    }

    // Creates the nodes without starting them, so a test can wire dependencies first. The
    // engine stops its nodes in its destructor, so it shuts down when the returned pointer goes
    // out of scope, also when a failed CATCH_REQUIRE throws.
    static std::unique_ptr< ConsensusEngine > setupEngine( const EngineOptions& _options = {} ) {
        if ( _options.extFace != nullptr )
            CATCH_REQUIRE( _options.lastCommittedBlockId >= 0 );

        ConsensusStartupOptions options;
        options.configDirectory = Consensust::getConfigDirPath();
        options.lastCommittedBlockId = _options.lastCommittedBlockId;
        options.storageLimitBytes = DEFAULT_STORAGE_LIMIT_BYTES;
        options.patchTimestamps = _options.patchTimestamps;
        options.externalFace = _options.extFace;
        options.lastCommittedTimestampS = _options.lastCommittedTimestampS;
        options.lastCommittedTimestampMs = _options.lastCommittedTimestampMs;
        options.useBlockIdFromConsensus = _options.useBlockIdFromConsensus;
        auto engine = setupConsensusEngine( options );

        if ( _options.requireSgx ) {
            for ( auto nodeId : ConsensusEngineTestAccess::getNodeIds( *engine ) ) {
                CATCH_INFO( "SGX test keys are required (run_sgx_test/sgx_data); node "
                            << nodeId << " uses mock crypto" );
                CATCH_REQUIRE(
                    ConsensusEngineTestAccess::getNode( *engine, nodeId )->isSgxEnabled() );
            }
        }
        return engine;
    }

    // Creates the nodes and starts the chain.
    static std::unique_ptr< ConsensusEngine > startEngine( const EngineOptions& _options = {} ) {
        auto engine = setupEngine( _options );
        engine->slowStartBootStrapTest();
        return engine;
    }

    // Starts the chain into _engine, waits _runTimeS seconds, and returns the highest block
    // committed in the database.
    static block_id startEngineAndWait(
        std::unique_ptr< ConsensusEngine >& _engine,
        int64_t _lastId,
        uint64_t _runTimeS,
        const std::map< string, uint64_t >& _patchTimestamps =
            ConsensusStartupOptions{}.patchTimestamps ) {
        CATCH_REQUIRE( _engine == nullptr );
        _engine = startEngine(
            { .lastCommittedBlockId = _lastId, .patchTimestamps = _patchTimestamps } );

        usleep( 1000 * 1000 * _runTimeS );

        CATCH_REQUIRE( _engine->nodesCount() > 0 );
        auto committedBlockIdInDb = _engine->getLargestCommittedBlockIDInDb();

        for ( int i = 0; i < 20 && committedBlockIdInDb == 0; i++ ) {
            usleep( 250 * 1000 );
            committedBlockIdInDb = _engine->getLargestCommittedBlockIDInDb();
        }

        return committedBlockIdInDb;
    }

    // ========= Environment variable helpers for tests ========= 
    static void setTestEnvVar(
        const string& _envName, const string& _envValue, int _overwrite = 1 ) {
        CATCH_REQUIRE( setenv( _envName.c_str(), _envValue.c_str(), _overwrite ) == 0 );
    }
};

// Sets an environment variable for the lifetime of the object. The destructor must not
// throw, so it restores the previous state directly instead of using CATCH_REQUIRE.
class ScopedTestEnvVar {
    string name;
    bool hadPreviousValue = false;
    string previousValue;

public:
    ScopedTestEnvVar( const string& _name, const string& _value ) : name( _name ) {
        if ( const char* previous = std::getenv( _name.c_str() ) ) {
            hadPreviousValue = true;
            previousValue = previous;
        }
        E2ETestHelper::setTestEnvVar( _name, _value );
    }

    ~ScopedTestEnvVar() {
        if ( hadPreviousValue ) {
            setenv( name.c_str(), previousValue.c_str(), 1 );
        } else {
            unsetenv( name.c_str() );
        }
    }

    ScopedTestEnvVar( const ScopedTestEnvVar& ) = delete;
    ScopedTestEnvVar& operator=( const ScopedTestEnvVar& ) = delete;
};

// Node identities of test/fournodes-same-ip; NODE_n has schain index n.
struct FourNodeTestChain {
    static constexpr const char* CONFIG_DIR = "test/fournodes-same-ip";
    inline static const node_id NODE_1 = node_id( 1112 );
    inline static const node_id NODE_2 = node_id( 1113 );
    inline static const node_id NODE_3 = node_id( 1114 );
    inline static const node_id NODE_4 = node_id( 1115 );
    inline static const std::vector< node_id > ALL_NODES = { NODE_1, NODE_2, NODE_3, NODE_4 };
};

// Callback for TestHooks::BlockCatchupResponseHook. The hook runs on the node that answers a
// catchup request. This callback makes it reply "no new blocks" to the catchup that _receiver does
// at startup (the request that starts at block 0), so _receiver has to commit its first blocks
// through consensus instead of downloading them. Later catchup requests are answered as usual.
// With no _receiver given, the startup catchup of every node is refused.
inline std::function< void( node_id, node_id, block_id, bool& ) > suppressInitialCatchup(
    std::optional< node_id > _receiver = std::nullopt ) {
    return [_receiver]( node_id, node_id receiverNodeId, block_id afterBlockId, bool& suppress ) {
        if ( afterBlockId == 0 && ( !_receiver || receiverNodeId == *_receiver ) )
            suppress = true;
    };
}

// Checks _condition every _interval until it is true or _timeout has passed, and returns
// whether it became true. Callers wrap it in CATCH_REQUIRE, so a failure reports the line of
// the wait that timed out.
template < typename Condition >
bool becomesTrueWithin(
    std::chrono::milliseconds _timeout, std::chrono::milliseconds _interval, Condition _condition ) {
    return waitUntil( _condition, deadlineIn( _timeout ), _interval ) == WaitResult::Satisfied;
}

// Returns the messages of _e and of every exception nested in it, one per line. Consensus code
// wraps exceptions with throw_with_nested, so the message of the check that failed is usually
// in a nested exception.
inline std::string nestedExceptionMessages( const std::exception& _e ) {
    std::string result = _e.what();
    try {
        std::rethrow_if_nested( _e );
    } catch ( const std::exception& nested ) {
        result += "\n" + nestedExceptionMessages( nested );
    } catch ( ... ) {
        result += "\nunknown exception";
    }
    return result;
}

}  // namespace E2ETestUtils
