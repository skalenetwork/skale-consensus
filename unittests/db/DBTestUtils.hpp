/*
    Copyright (C) 2019 SKALE Labs

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

    @file DBTestUtils.hpp
    @author SKALE Labs
    @date 2026
*/

#pragma once

#include <boost/filesystem.hpp>

#include "SkaleCommon.h"
#include "chains/Schain.h"
#include "node/ConsensusEngine.h"
#include "node/Node.h"
#include "json/JSONFactory.h"
#include "unittests/TestUtils.h"

namespace DBTestUtils {

namespace fs = boost::filesystem;

static const string TEST_DB_DIR = "/tmp";

// ============== Test Fixture ============== //

/** 
 * Singleton session-lifetime test fixture
 * Engine/Node/Schain are created once and reused across all DB tests
 * This avoids heavyweight initialization and shutdown per-test
 */
struct TestFixture {
    ptr< ConsensusEngine > engine;
    ptr< Node > node;
    ptr< Schain > schain;
    
    TestFixture() {
        // Create minimal ConsensusEngine (no extFace needed for DB tests)
        engine = make_shared< ConsensusEngine >( block_id( 0 ), 1000000000 );

        // Create Node and Schain using centralized test helper.
        // Use a non-empty geth URL to avoid noisy warning logs during DB tests.
        TestUtils::createTestNodeAndSchain(
            node, schain, *engine,
            node_id( 1 ), "TestNode", "127.0.0.1", 1231,
            schain_id( 1 ), "TestChain", schain_index( 1 ), "http://127.0.0.1:8545" );
    }
    
    Schain* getSchain() {
        CHECK_STATE( schain );
        return schain.get();
    }
};

/**
 * @brief Keeps session-lifetime TestFixture instance.
 * Avoids creating & destroying chains for each test.
 */
inline TestFixture& getSharedFixture() {
    static TestFixture fixture;
    return fixture;
}

// ============== DB Creation/Cleanup Helpers ============== //

/**
 * @brief Helper function to remove all files starting with `fileName` values inside the `dbPath` directory. Used for cleaning up DB files before/after tests.
 */
inline void removeDBFiles( const fs::path& dbPath, const string& fileName ) {
    if ( fs::exists( dbPath ) ) {
        // Remove files and subdirectories matching the pattern
        for ( fs::directory_iterator it( dbPath ), end; it != end; ++it ) {
            if ( it->path().filename().string().find( fileName ) == 0 ) {
                if ( fs::is_directory( it->path() ) ) {
                    fs::remove_all( it->path() );
                } else {
                    fs::remove( it->path() );
                }
            }
        }
    }
}

template < typename DBType >
ptr< DBType > createDB( const string& _dbName ) {
    auto sChain = getSharedFixture().getSchain();
    
    string dirName = TEST_DB_DIR;
    string fileName = _dbName;

    // Clean up DB files/directories before test
    fs::path dbPath( dirName );
    removeDBFiles( dbPath, fileName );

    return make_shared< DBType >( sChain, dirName, fileName, node_id( 1 ), 5000000 );
}

template < typename DBType >
ptr< DBType > reopenDB( const string& _dbName ) {
    auto sChain = getSharedFixture().getSchain();
    
    string dirName = TEST_DB_DIR;
    string fileName = _dbName;

    // Do NOT clean up - reopen existing DB
    return make_shared< DBType >( sChain, dirName, fileName, node_id( 1 ), 5000000 );
}

inline void cleanupDB( const string& _dbName ) {
    // Only remove DB files, NOT engine/node
    // Engine/Node/Schain persist for entire test session to avoid shutdown cost
    string dirName = TEST_DB_DIR;
    string fileName = _dbName;

    fs::path dbPath( dirName );
    removeDBFiles( dbPath, fileName );
}

}  // namespace DBTestUtils
