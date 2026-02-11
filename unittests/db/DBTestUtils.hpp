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
#include "thirdparty/json.hpp"

namespace DBTestUtils {

namespace fs = boost::filesystem;

static const string TEST_DB_DIR = "/tmp";

// Singleton session-lifetime test fixture
// Engine/Node/Schain are created once and reused across all DB tests
// This avoids heavyweight initialization and shutdown per-test
struct TestFixture {
    ptr< ConsensusEngine > engine;
    ptr< Node > node;
    Schain* schain = nullptr;
    
    TestFixture() {
        // Lazy initialization on first use
        if ( !engine ) {
            // Create minimal ConsensusEngine (no extFace needed for DB tests)
            engine = make_shared< ConsensusEngine >( block_id( 0 ), 1000000000 );
            
            // Create minimal Node JSON config
            nlohmann::json nodeConfig = {
                { "nodeName", "TestNode" },
                { "nodeID", 1 },
                { "bindIP", "127.0.0.1" },
                { "basePort", 1231 },
                { "isTestNet", 1 }
            };
            
            // Create the Node
            set< node_id > nodeIDs;
            string gethURL = "";
            node = JSONFactory::createNodeFromJsonObject( nodeConfig, nodeIDs, engine.get(),
                false, "", "", "", "", nullptr, "", nullptr, nullptr, gethURL, nullptr, nullptr, nullptr );
            
            // Create minimal Schain JSON config with single node
            nlohmann::json schainConfig = {
                { "schainName", "TestChain" },
                { "schainID", 1 },
                { "nodes", nlohmann::json::array( {
                    { { "nodeID", 1 }, { "ip", "127.0.0.1" }, { "basePort", 1231 }, { "schainIndex", 1 } }
                } ) },
                { "blockProposalTest", "NONE" }
            };
            
            // Initialize the Schain from the config
            JSONFactory::createAndAddSChainFromJsonObject( node, schainConfig, engine.get() );
            
            schain = node->getSchain();
        }
    }
    
    Schain* getSchain() {
        if ( !schain ) {
            // Trigger lazy initialization if needed
            TestFixture();
        }
        return schain;
    }
};

inline TestFixture& getTestFixture() {
    // Intentionally leaked process-lifetime fixture:
    // DB unit tests need a live Schain/Node/Engine context, and ConsensusEngine
    // teardown can block during global/static destruction at process exit.
    // Keeping this fixture alive until process termination avoids that shutdown path.
    static TestFixture* fixture = new TestFixture();
    return *fixture;
}

template < typename DBType >
ptr< DBType > createDB( const string& _dbName ) {
    // Use shared session-lifetime Schain
    auto sChain = getTestFixture().getSchain();
    
    string dirName = TEST_DB_DIR;
    string fileName = _dbName;

    // Clean up DB files/directories before test
    fs::path dbPath( dirName );
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

    return make_shared< DBType >( sChain, dirName, fileName, node_id( 1 ), 5000000 );
}

template < typename DBType >
ptr< DBType > reopenDB( const string& _dbName ) {
    // Use shared session-lifetime Schain
    auto sChain = getTestFixture().getSchain();
    
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

}  // namespace DBTestUtils
