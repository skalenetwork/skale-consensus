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
#include "tests/TestUtils.h"
#include "tests/harness/consensus/ConsensusStartup.h"

#include <boost/filesystem.hpp>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <optional>

namespace E2ETestUtils {

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

    static void setupEngine(
        ConsensusEngine*& _engine,
        int64_t _lastId,
        const std::map< string, uint64_t >& _patchTimestamps =
            ConsensusStartupOptions{}.patchTimestamps,
        ConsensusExtFace* _extFace = nullptr,
        uint64_t _lastCommittedBlockTimeStamp = 0,
        uint64_t _lastCommittedBlockTimeStampMs = 0,
        bool _useBlockIDFromConsensus = false ) {
        CATCH_REQUIRE( _engine == nullptr );

        if ( _extFace != nullptr )
            CATCH_REQUIRE( _lastId >= 0 );

        ConsensusStartupOptions options;
        options.configDirectory = Consensust::getConfigDirPath();
        options.lastCommittedBlockId = _lastId;
        options.storageLimitBytes = DEFAULT_STORAGE_LIMIT_BYTES;
        options.patchTimestamps = _patchTimestamps;
        options.externalFace = _extFace;
        options.lastCommittedTimestampS = _lastCommittedBlockTimeStamp;
        options.lastCommittedTimestampMs = _lastCommittedBlockTimeStampMs;
        options.useBlockIdFromConsensus = _useBlockIDFromConsensus;
        _engine = setupConsensusEngine( options ).release();
    }

    static void startEngine(
        ConsensusEngine*& _engine,
        int64_t _lastId,
        const std::map< string, uint64_t >& _patchTimestamps =
            ConsensusStartupOptions{}.patchTimestamps,
        ConsensusExtFace* _extFace = nullptr,
        uint64_t _lastCommittedBlockTimeStamp = 0,
        uint64_t _lastCommittedBlockTimeStampMs = 0,
        bool _useBlockIDFromConsensus = false ) {
        setupEngine( _engine, _lastId, _patchTimestamps, _extFace,
            _lastCommittedBlockTimeStamp, _lastCommittedBlockTimeStampMs,
            _useBlockIDFromConsensus );
        _engine->slowStartBootStrapTest();
    }

    static block_id startEngineAndWait(
        ConsensusEngine*& _engine,
        int64_t _lastId,
        uint64_t _runTimeS,
        const std::map< string, uint64_t >& _patchTimestamps =
            ConsensusStartupOptions{}.patchTimestamps ) {
        startEngine( _engine, _lastId, _patchTimestamps );

        usleep( 1000 * 1000 * _runTimeS );

        CATCH_REQUIRE( _engine->nodesCount() > 0 );
        auto committedBlockIdInDb = _engine->getLargestCommittedBlockIDInDb();

        for ( int i = 0; i < 20 && committedBlockIdInDb == 0; i++ ) {
            usleep( 250 * 1000 );
            committedBlockIdInDb = _engine->getLargestCommittedBlockIDInDb();
        }

        return committedBlockIdInDb;
    }

    static void stopEngineGracefully( ConsensusEngine*& _engine ) {
        _engine->testExitGracefullyBlocking();
        delete _engine;
        _engine = nullptr;
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

// Stops the engine on scope exit if the test did not already do so. Declare it before any
// hook scopes the engine depends on, so hooks are removed first and cannot block shutdown.
class ScopedEngineShutdown {
    ConsensusEngine*& engine;

public:
    explicit ScopedEngineShutdown( ConsensusEngine*& _engine ) : engine( _engine ) {}

    ~ScopedEngineShutdown() {
        if ( engine == nullptr )
            return;
        try {
            E2ETestHelper::stopEngineGracefully( engine );
        } catch ( ... ) {
        }
    }

    ScopedEngineShutdown( const ScopedEngineShutdown& ) = delete;
    ScopedEngineShutdown& operator=( const ScopedEngineShutdown& ) = delete;
};

// Node identities of test/fournodes-same-ip; NODE_n has schain index n.
struct FourNodeTestChain {
    static constexpr const char* CONFIG_DIR = "test/fournodes-same-ip";
    inline static const node_id NODE_1 = node_id( 1112 );
    inline static const node_id NODE_2 = node_id( 1113 );
    inline static const node_id NODE_3 = node_id( 1114 );
    inline static const node_id NODE_4 = node_id( 1115 );
};

// Callback for TestHooks::BlockCatchupResponseHook. Without it, catch-up could deliver a block
// that a finalization test expects the receiver to fail to commit by itself. With no receiver
// given, initial catch-up is suppressed for every node.
inline std::function< void( node_id, node_id, block_id, bool& ) > suppressInitialCatchup(
    std::optional< node_id > _receiver = std::nullopt ) {
    return [_receiver]( node_id, node_id receiverNodeId, block_id afterBlockId, bool& suppress ) {
        if ( afterBlockId == 0 && ( !_receiver || receiverNodeId == *_receiver ) )
            suppress = true;
    };
}

}  // namespace E2ETestUtils
