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

#include <boost/filesystem.hpp>
#include <cstdio>
#include <map>

namespace E2ETestUtils {

class E2ETestHelper {
public:
    inline static const string TEST_DATA_ROOT_DIR = "/tmp/consensus_tests";
    inline static const string ONE_NODE_TEST_DATA_DIR = TEST_DATA_ROOT_DIR + "/onenode";
    inline static const string TWO_NODE_TEST_DATA_DIR = TEST_DATA_ROOT_DIR + "/twonodes_sameip";
    static constexpr uint64_t DEFAULT_RUN_TIME_S = 4;
    static constexpr uint64_t CROSS_NODE_RUN_TIME_S = 20;
    static constexpr uint64_t DEFAULT_STORAGE_LIMIT_BYTES = 1000000000;

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

        TestUtils::setTestEnvVar( "DATA_DIR", TEST_DATA_ROOT_DIR.c_str() );

        // Keep default runtime for fast tests unless caller already set TEST_TIME_S.
        TestUtils::setTestEnvVar( "TEST_TIME_S", "4", 0 );
    }

    static void startEngine(
        ConsensusEngine*& _engine,
        int64_t _lastId,
        const std::map< string, uint64_t >& _patchTimestamps = {
#ifdef BITE
            { "bite2PatchTimestamp", 0 }
#endif
        },
        ConsensusExtFace* _extFace = nullptr,
        uint64_t _lastCommittedBlockTimeStamp = 0,
        uint64_t _lastCommittedBlockTimeStampMs = 0,
        bool _useBlockIDFromConsensus = false ) {
        CATCH_REQUIRE( _engine == nullptr );

        if ( _extFace != nullptr ) {
            CATCH_REQUIRE( _lastId >= 0 );
            _engine = new ConsensusEngine( *_extFace, (uint64_t) _lastId,
                _lastCommittedBlockTimeStamp, _lastCommittedBlockTimeStampMs, _patchTimestamps,
                DEFAULT_STORAGE_LIMIT_BYTES );
        } else {
            _engine = new ConsensusEngine( _lastId, DEFAULT_STORAGE_LIMIT_BYTES );
            if ( !_patchTimestamps.empty() ) {
                _engine->setTestPatchTimestamps( _patchTimestamps );
            }
        }

        auto useBlockIdFromConsensus = _useBlockIDFromConsensus || ( _extFace == nullptr && _lastId == -1 );
        _engine->parseTestConfigsAndCreateAllNodes(
            Consensust::getConfigDirPath(), useBlockIdFromConsensus );
        _engine->slowStartBootStrapTest();
    }

    static block_id startEngineAndWait(
        ConsensusEngine*& _engine,
        int64_t _lastId,
        uint64_t _runTimeS,
        const std::map< string, uint64_t >& _patchTimestamps = { // default patch timestamps
#ifdef BITE
            { "bite2PatchTimestamp", 0 }
#endif 
        } ) {
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
};

}  // namespace E2ETestUtils
