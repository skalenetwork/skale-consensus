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

    @file RandomConsensusTests.cpp
    @author SKALE Labs
    @date 2026
*/

#ifdef BITE2

#include "thirdparty/catch.hpp"
#include "Consensust.h"
#include "SkaleCommon.h"
#include "datastructures/CommittedBlock.h"
#include "node/ConsensusEngine.h"
#include "utils/Time.h"
#include "unittests/TestUtils.h"

#include <boost/filesystem.hpp>
#include <cstdlib>
#include <fstream>
#include <map>
#include <unordered_set>
#include <vector>

namespace RandomTests {

static const string TEST_DATA_DIR = "/tmp/consensus_reencryption_random_tests";
static const string RESTART_SNAPSHOT_FILE = "/tmp/reencryption_randoms_pre_restart.txt";

void configureTestEnvironment( bool _cleanDataDir, const string& _configDir = "" ) {
    auto cfgDir = boost::filesystem::system_complete( _configDir.empty() ? "test/onenode" : _configDir );
    CATCH_REQUIRE( boost::filesystem::exists( cfgDir ) );
    CATCH_REQUIRE( boost::filesystem::is_directory( cfgDir ) );
    Consensust::setConfigDirPath( cfgDir );

    if ( _cleanDataDir ) {
        boost::filesystem::remove_all( TEST_DATA_DIR );
        std::remove( RESTART_SNAPSHOT_FILE.c_str() );
    }

    boost::filesystem::create_directories( TEST_DATA_DIR );

    TestUtils::setTestEnvVar( "DATA_DIR", TEST_DATA_DIR.c_str() );
    TestUtils::setTestEnvVar( "LOG_DIR", TEST_DATA_DIR.c_str() );

    // consensus running time to 4s - dont replace if already set (0)
    // to produce some blocks
    TestUtils::setTestEnvVar( "TEST_TIME_S", "4", 0 );
}

/**
 * @brief Helper to start consensus engine and wait for committed blocks
 * 
 * @param engine Pointer to engine (will be allocated)
 * @param lastId Starting block ID (0 for fresh start, -1 for continue)
 * @param runTimeS How long to run in seconds
 * @return block_id The largest committed block ID that is persisted in DB
 */
block_id startEngineAndWait( ConsensusEngine*& engine, int64_t lastId, uint64_t runTimeS ) {
    engine = new ConsensusEngine( lastId, 1000000000 );
#ifdef BITE2
    engine->setTestPatchTimestamps( { { "bite2PatchTimestamp", 1 } } );
#endif
    engine->parseTestConfigsAndCreateAllNodes( Consensust::getConfigDirPath(), lastId == -1 );
    engine->slowStartBootStrapTest();
    
    // Wait for blocks to be committed
    usleep( 1000 * 1000 * runTimeS );
    
    CATCH_REQUIRE( engine->nodesCount() > 0 );
    auto committedBlockId = engine->getLargestCommittedBlockID();
    auto committedBlockIdInDb = engine->getLargestCommittedBlockIDInDb();

    // Reencryption random reads are guarded by DB committed height; allow short catch-up window.
    for ( int i = 0; i < 20 && committedBlockId > 0 && committedBlockIdInDb == 0; i++ ) {
        usleep( 250 * 1000 );
        committedBlockIdInDb = engine->getLargestCommittedBlockIDInDb();
    }
    
    return committedBlockIdInDb;
}

/**
 * @brief Gracefully stop the consensus engine
 * 
 * @param engine Pointer to engine (will be deleted)
 */
void stopEngineGracefully( ConsensusEngine*& engine ) {
    engine->testExitGracefullyBlocking();
    delete engine;
    engine = nullptr;
}

/**
 * @brief Read reencryption randoms for a range of block IDs
 * 
 * @param engine The consensus engine
 * @param startBlockId Starting block ID (inclusive)
 * @param endBlockId Ending block ID (inclusive)
 * @return std::map<uint64_t, u256> Map of block ID to reencryption random
 */
std::map< uint64_t, u256 > readReencryptionRandoms(
    ConsensusEngine* engine, uint64_t startBlockId, uint64_t endBlockId ) {
    std::map< uint64_t, u256 > randoms;
    
    for ( uint64_t blockId = startBlockId; blockId <= endBlockId; blockId++ ) {
        try {
            auto random = engine->getReencryptionRandomForBlockId( blockId );
            randoms[blockId] = random;
        } catch ( ... ) {
            // Block may not have reencryption random yet (very recent or not committed)
        }
    }
    
    return randoms;
}

}  // namespace RandomTests

using namespace RandomTests;

CATCH_TEST_CASE(
    "getReencryptionRandomForBlockId returns for committed blocks",
    "[reencryption-random-committed][end-to-end][db][bite2]" ) {
    
    ConsensusEngine* testEngine = nullptr;
    
    try {
        configureTestEnvironment( true );
        // Start consensus and wait for blocks to be committed
        auto lastId = startEngineAndWait( testEngine, 0, Consensust::getRunningTimeS() );
        
        CATCH_REQUIRE( lastId > 0 );
        
        auto randoms = readReencryptionRandoms( testEngine, 1, (uint64_t) lastId );
        
        // Assert at least one successful read
        CATCH_REQUIRE( randoms.size() > 0 );
        
        // all randoms should be different
        std::unordered_set<u256> uniqueRandoms;

        // Verify each random is non-zero (valid)
        for ( const auto& [blockId, random] : randoms ) {
            CATCH_REQUIRE( random != u256( 0 ) );

            // insert fails only if the random is already in the set, which would indicate a duplicate
            if ( uniqueRandoms.insert( random ).second == false ) {
                CATCH_FAIL( "Duplicate reencryption random found for block ID " + std::to_string( blockId ) );
            }

        }
        
        stopEngineGracefully( testEngine );
        
    } catch ( SkaleException& e ) {
        if ( testEngine ) {
            stopEngineGracefully( testEngine );
        }
        SkaleException::logNested( e );
        throw;
    }
    
    CATCH_SUCCEED();
}

CATCH_TEST_CASE(
    "repeated reads for same block are equal",
    "[reencryption-random-deterministic][end-to-end][db][bite2]" ) {
    
    ConsensusEngine* testEngine = nullptr;
    
    try {
        configureTestEnvironment( true );
        // Start consensus and wait for blocks to be committed
        auto lastId = startEngineAndWait( testEngine, 0, Consensust::getRunningTimeS() );
        
        CATCH_REQUIRE( lastId > 0 );
        
        // Read getReencryptionRandomForBlockId(id) twice
        u256 random1;
        u256 random2;
        
        try {
            random1 = testEngine->getReencryptionRandomForBlockId( (uint64_t) lastId );
            random2 = testEngine->getReencryptionRandomForBlockId( (uint64_t) lastId );
            
            // Assert equality
            CATCH_REQUIRE( random1 == random2 );
            CATCH_REQUIRE( random1 != u256( 0 ) );
            
        } catch ( ... ) {
            CATCH_FAIL( "Failed to read reencryption random for block ID " + std::to_string( (uint64_t) lastId ) );
        }
        
        stopEngineGracefully( testEngine );
        
    } catch ( SkaleException& e ) {
        if ( testEngine ) {
            stopEngineGracefully( testEngine );
        }
        SkaleException::logNested( e );
        throw;
    }
    
    CATCH_SUCCEED();
}

CATCH_TEST_CASE(
    "reencryption random differs from public random",
    "[reencryption-random-vs-public][end-to-end][db][bite2]" ) {
    
    ConsensusEngine* testEngine = nullptr;
    
    try {
        configureTestEnvironment( true );
        // Start consensus and wait for blocks to be committed
        auto lastId = (uint64_t) startEngineAndWait( testEngine, 0, Consensust::getRunningTimeS() );
        
        CATCH_REQUIRE( lastId > 0 );
        
        uint64_t checkedBlocks = 0;
        uint64_t startBlock = 1;
#ifdef BITE2
        startBlock = 2;
#endif
        
        for ( uint64_t blockId = startBlock; blockId <= lastId; blockId++ ) {
            try {
                // For same committed block id, compare:
                // getReencryptionRandomForBlockId(id) vs getRandomForBlockId(id)
                auto reencryptionRandom = testEngine->getReencryptionRandomForBlockId( blockId );
                auto publicRandom = testEngine->getRandomForBlockId( blockId );
                
                // Assert they are different (domain separation)
                CATCH_REQUIRE( reencryptionRandom != publicRandom );
                CATCH_REQUIRE( reencryptionRandom != u256( 0 ) );
                CATCH_REQUIRE( publicRandom != u256( 0 ) );
                
                checkedBlocks++;
                
            } catch ( SkaleException& e ) {
                CATCH_FAIL( "Failed to read randoms for block ID " + std::to_string( blockId ) + ": " + e.what() );
            }
        }
        
        // Ensure we checked at least some blocks
        CATCH_REQUIRE( checkedBlocks > 0 );
        
        stopEngineGracefully( testEngine );
        
    } catch ( SkaleException& e ) {
        if ( testEngine ) {
            stopEngineGracefully( testEngine );
        }
        SkaleException::logNested( e );
        throw;
    }
    
    CATCH_SUCCEED();
}

CATCH_TEST_CASE(
    "reencryption random survives restart - first run",
    "[reencryption-random-restart-1][end-to-end][db][bite2]" ) {
    
    ConsensusEngine* testEngine = nullptr;
    
    try {
        configureTestEnvironment( true );
        // First run: start from scratch and collect reencryption randoms
        auto lastId = (uint64_t) startEngineAndWait( testEngine, 0, Consensust::getRunningTimeS() );
        
        CATCH_REQUIRE( lastId > 0 );
        
        auto randomsBeforeRestart = readReencryptionRandoms( testEngine, 1, lastId );
        
        CATCH_REQUIRE( randomsBeforeRestart.size() > 0 );
        
        // Store the randoms to a temporary file for the next test to read
        // (In a real test, this would be stored in a shared location or environment)
        std::ofstream outFile( RESTART_SNAPSHOT_FILE );

        outFile << lastId << "\n";
        for ( const auto& [blockId, random] : randomsBeforeRestart ) {
            outFile << blockId << " " << random << "\n";
        }
        outFile.close();
        
        // Graceful shutdown
        stopEngineGracefully( testEngine );
        
    } catch ( SkaleException& e ) {
        if ( testEngine ) {
            stopEngineGracefully( testEngine );
        }
        SkaleException::logNested( e );
        throw;
    }
    
    CATCH_SUCCEED();
}

CATCH_TEST_CASE(
    "reencryption random survives restart - continue after restart",
    "[reencryption-random-restart-2][end-to-end][bite2]" ) {
    
    ConsensusEngine* testEngine = nullptr;
    
    try {
        configureTestEnvironment( false );
        // Load the randoms from before restart
        std::ifstream inFile( RESTART_SNAPSHOT_FILE );
        CATCH_REQUIRE( inFile.is_open() );
        
        std::map< uint64_t, u256 > randomsBeforeRestart;
        uint64_t previousLastId;
        inFile >> previousLastId;
        
        std::string blockIdStr, randomStr;
        while ( inFile >> blockIdStr >> randomStr ) {
            uint64_t blockId = std::stoull( blockIdStr );
            u256 random( randomStr );
            randomsBeforeRestart[blockId] = random;
        }
        inFile.close();
        
        CATCH_REQUIRE( randomsBeforeRestart.size() > 0 );
        
        // Restart with continue mode (_lastId == -1 path)
        testEngine = new ConsensusEngine( -1, 1000000000 );
        testEngine->parseTestConfigsAndCreateAllNodes( Consensust::getConfigDirPath(), true );
        testEngine->slowStartBootStrapTest();
        
        // Give it 2s to initialize
        usleep( 1000 * 1000 * 2 );
        
        CATCH_REQUIRE( testEngine->nodesCount() > 0 );
        
        // Read the same block IDs and assert values match pre-restart
        uint64_t matchedBlocks = 0;
        for ( const auto& [blockId, expectedRandom] : randomsBeforeRestart ) {
            try {
                auto actualRandom = testEngine->getReencryptionRandomForBlockId( blockId );
                CATCH_REQUIRE( actualRandom == expectedRandom );
                matchedBlocks++;
            } catch ( ... ) {
                CATCH_FAIL( "Failed to read reencryption random after restart for block " 
                    + std::to_string( blockId ) );
            }
        }
        
        // Ensure we matched all blocks
        CATCH_REQUIRE( matchedBlocks == randomsBeforeRestart.size() );
        
        stopEngineGracefully( testEngine );
        
        // Cleanup the temporary file
        std::remove( RESTART_SNAPSHOT_FILE.c_str() );
        
    } catch ( SkaleException& e ) {
        if ( testEngine ) {
            stopEngineGracefully( testEngine );
        }
        SkaleException::logNested( e );
        throw;
    }
    
    CATCH_SUCCEED();
}

CATCH_TEST_CASE(
    "reencryption random is equal across nodes for same block ids",
    "[reencryption-random-cross-node][end-to-end][db]" ) {

    ConsensusEngine* testEngine = nullptr;

    try {
        configureTestEnvironment( true, "test/twonodes_sameip" );
        auto runTimeS = std::max< uint64_t >( Consensust::getRunningTimeS(), 10 );
        auto lastId =
            ( uint64_t ) startEngineAndWait( testEngine, 0, runTimeS );
        auto lastIdInMemory = ( uint64_t ) testEngine->getLargestCommittedBlockID();

        // Two-node setup can need extra time before first DB commit; poll briefly before failing.
        for ( int i = 0; i < 70 && lastId == 0; i++ ) {
            usleep( 500 * 1000 );
            lastId = ( uint64_t ) testEngine->getLargestCommittedBlockIDInDb();
            lastIdInMemory = ( uint64_t ) testEngine->getLargestCommittedBlockID();
        }

        CATCH_INFO(
            "lastIdInDb=" << lastId << ", lastIdInMemory=" << lastIdInMemory << ", runTimeS=" << runTimeS );
        CATCH_REQUIRE( lastId > 0 );
        CATCH_REQUIRE( ( uint64_t ) testEngine->nodesCount() == 2 );

        auto nodeIds = testEngine->getNodeIDs();
        CATCH_REQUIRE( nodeIds.size() == 2 );

        auto nodeIt = nodeIds.begin();
        auto firstNodeId = *nodeIt;
        ++nodeIt;
        auto secondNodeId = *nodeIt;

        std::map< uint64_t, u256 > firstNodeRandoms;
        std::map< uint64_t, u256 > secondNodeRandoms;

        for ( uint64_t blockId = 1; blockId <= lastId; blockId++ ) {
            try {
                firstNodeRandoms[blockId] =
                    testEngine->getReencryptionRandomForBlockIdForNode( blockId, firstNodeId );
            } catch ( ... ) {}

            try {
                secondNodeRandoms[blockId] =
                    testEngine->getReencryptionRandomForBlockIdForNode( blockId, secondNodeId );
            } catch ( ... ) {}
        }

        uint64_t comparedBlocks = 0;
        for ( const auto& [blockId, firstRandom] : firstNodeRandoms ) {
            if ( secondNodeRandoms.count( blockId ) == 0 ) {
                continue;
            }

            CATCH_REQUIRE( firstRandom == secondNodeRandoms.at( blockId ) );
            comparedBlocks++;
        }

        CATCH_REQUIRE( comparedBlocks > 0 );

        stopEngineGracefully( testEngine );
    } catch ( SkaleException& e ) {
        if ( testEngine ) {
            stopEngineGracefully( testEngine );
        }
        SkaleException::logNested( e );
        throw;
    }

    CATCH_SUCCEED();
}

CATCH_TEST_CASE(
    "reencryption random/signature transition across bite2 patch timestamp",
    "[reencryption-signature-transition][bite2][end-to-end][db]" ) {
    
    ConsensusEngine* testEngine = nullptr;
    
    try {
        configureTestEnvironment( true );

        auto snap = TestUtils::setTestEnvVar("TEST_TRANSACTIONS_PER_BLOCK", "1");

        uint64_t runTimeS = Consensust::getRunningTimeS();
        if ( runTimeS < 8 ) {
            runTimeS = 8;
        }
        static constexpr uint64_t PATCH_DELAY_MS = 2000;
        auto bite2PatchTimestamp = ( Time::getCurrentTimeMs() + PATCH_DELAY_MS + 999 ) / 1000;
        
        testEngine = new ConsensusEngine( 0, 1000000000 );
        testEngine->setTestPatchTimestamps( { { "bite2PatchTimestamp", bite2PatchTimestamp } } );
        testEngine->parseTestConfigsAndCreateAllNodes( Consensust::getConfigDirPath(), false );
        testEngine->slowStartBootStrapTest();
        
        // Wait for blocks to be committed
        usleep( 1000 * 1000 * runTimeS );
        
        auto lastId = testEngine->getLargestCommittedBlockIDInDb();
        
        // Poll briefly if DB commit is still pending
        for ( int i = 0; i < 20 && lastId == 0; i++ ) {
            usleep( 250 * 1000 );
            lastId = testEngine->getLargestCommittedBlockIDInDb();
        }
        
        CATCH_REQUIRE( lastId > 0 );
        
        uint64_t checkedBlocks = 0;
        
        // Access schain directly to get blocks
        auto nodeIds = testEngine->getNodeIDs();
        CATCH_REQUIRE( nodeIds.size() > 0 );
        
        uint64_t checkedBeforePatch = 0;
        uint64_t checkedAfterPatch = 0;

        // Validate transition by block timestamp: 
        // before patch -> absent, 
        // after patch -> present.
        for ( uint64_t blockId = 1; blockId <= (uint64_t)lastId; blockId++ ) {
            try {
                auto block = testEngine->getCommittedBlockForBlockId( blockId );
                CATCH_REQUIRE( block != nullptr );
                auto blockTimestamp = block->getTimeStampS();
                auto reencryptionSignature = block->getReencryptionThresholdSig();

                CATCH_INFO( "blockId=" << blockId << ", blockTimestamp=" << blockTimestamp <<
                    ", bite2PatchTimestamp=" << bite2PatchTimestamp );

                // Before patch timestamp
                if ( blockTimestamp < bite2PatchTimestamp ) {
                    // committed block has no reencryption signature
                    bool noReencryptionSignature =
                        !reencryptionSignature.has_value() || reencryptionSignature->empty();
                    CATCH_REQUIRE( noReencryptionSignature );

                    // db read for reencryption random should fail (not available before patch)
                    bool randomReadFailed = false;
                    try {
                        auto random = testEngine->getReencryptionRandomForBlockId( blockId );
                        (void) random;
                    } catch ( ... ) {
                        randomReadFailed = true;
                    }
                    CATCH_REQUIRE( randomReadFailed );
                    checkedBeforePatch++;
                } else {
                    // After patch timestamp
                    CATCH_REQUIRE( reencryptionSignature.has_value() );
                    CATCH_REQUIRE( !reencryptionSignature->empty() );
                    // reencryption random should be saved in DB, readable, and non-zero
                    auto reencryptionRandom = testEngine->getReencryptionRandomForBlockId( blockId );
                    CATCH_REQUIRE( reencryptionRandom != u256( 0 ) );
                    checkedAfterPatch++;
                }
                checkedBlocks++;
            } catch ( const SkaleException& e ) {
                CATCH_FAIL( "Exception thrown while checking block ID " + std::to_string( blockId ) +
                    " for reencryption random/signature transition: " + e.what() );
            } catch ( ... ) {
                CATCH_FAIL( "Unknown exception thrown while checking block ID " +
                    std::to_string( blockId ) + " for reencryption random/signature transition." );
            }
        }
        
        CATCH_REQUIRE( checkedBlocks == lastId );
        CATCH_REQUIRE( checkedBeforePatch > 0 );
        CATCH_REQUIRE( checkedAfterPatch > 0 );
        
        stopEngineGracefully( testEngine );

        TestUtils::restoreTestEnvVar( "TEST_TRANSACTIONS_PER_BLOCK", snap );
        
    } catch ( SkaleException& e ) {
        if ( testEngine ) {
            stopEngineGracefully( testEngine );
        }
        SkaleException::logNested( e );
        throw;
    } catch ( ... ) {
        if ( testEngine ) {
            stopEngineGracefully( testEngine );
        }
        throw;
    }
    
    CATCH_SUCCEED();
}

#endif  // BITE2
