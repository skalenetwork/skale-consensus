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

    @file DBTestUtils.cpp
    @author SKALE Labs
    @date 2026
*/

#include "SkaleCommon.h"
#include "thirdparty/json.hpp"
#include "node/Node.h"
#include "node/NodeInfo.h"
#include "chains/Schain.h"

namespace DBTestUtils {

static const string TEST_DB_DIR = "/tmp";

// Create a minimal JSON config for test Node
static nlohmann::json createMinimalTestConfig() {
    nlohmann::json cfg;
    cfg["nodeID"] = 1;
    cfg["bindIP"] = "127.0.0.1";
    cfg["basePort"] = 1234;
    cfg["httpRpcPort"] = 3000;
    cfg["httpsRpcPort"] = 3001;
    cfg["wsRpcPort"] = 3002;
    cfg["wssRpcPort"] = 3003;
    cfg["logLevel"] = "info";
    cfg["logLevelProposal"] = "info";
    cfg["maxDBSize"] = 5000000;
    return cfg;
}

// Create a minimal Node for DB testing
static ptr< Node > createTestNode() {
    auto cfg = createMinimalTestConfig();
    
    // Prepare all required parameters for Node constructor
    string sgxURL = "";
    string sgxSSLKeyFileFullPath = "";
    string sgxSSLCertFileFullPath = "";
    string ecdsaKeyName = "";
    ptr< vector< string > > ecdsaPublicKeys = nullptr;
    string blsKeyName = "";
    ptr< vector< ptr< vector< string > > > > blsPublicKeys = nullptr;
    ptr< libBLS::BLSPublicKey > blsPublicKey = nullptr;
    string gethURL = "";
    ptr< map< uint64_t, ptr< libBLS::BLSPublicKey > > > previousBlsPublicKeys = nullptr;
    ptr< map< uint64_t, string > > historicECDSAPublicKeys = nullptr;
    ptr< map< uint64_t, vector< uint64_t > > > historicNodeGroups = nullptr;
    bool isSyncNode = false;
    
    // Create Node with all required parameters
    auto node = make_shared< Node >( 
        cfg, nullptr, false,
        sgxURL, sgxSSLKeyFileFullPath, sgxSSLCertFileFullPath,
        ecdsaKeyName, ecdsaPublicKeys, blsKeyName,
        blsPublicKeys, blsPublicKey,
        gethURL, previousBlsPublicKeys,
        historicECDSAPublicKeys,
        historicNodeGroups, isSyncNode 
    );
    
    return node;
}

// Create a minimal Schain attached to a test Node
static Schain* createTestSchainWithNode( ptr< Node > _node ) {
    schain_id schainID = 1;
    schain_index schainIndex = 1;
    string schainName = "TestSchain";

    // Create minimal node infos for a 4-node test chain
    vector< ptr< NodeInfo > > nodeInfos;
    for ( int i = 1; i <= 4; i++ ) {
        auto nodeInfo = make_shared< NodeInfo >(
            node_id( i ), "127.0.0.1", network_port( 1234 + i * 10 ), schainID, schain_index( i ) );
        nodeInfos.push_back( nodeInfo );
    }

    // Initialize the schain
    Node::initSchain( _node, schainIndex, schainID, nodeInfos, nullptr, schainName );

    return _node->getSchain();
}

// Store test nodes to keep them alive
static map< string, ptr< Node > > testNodes;

template < typename DBType >
ptr< DBType > createDB( const string& _dbName ) {
    auto node = createTestNode();
    auto sChain = createTestSchainWithNode( node );
    
    // Keep node alive for the lifetime of this DB
    testNodes[_dbName] = node;
    
    string dirName = TEST_DB_DIR;
    string fileName = _dbName;

    // Clean up before test
    if ( std::system( ( "rm -rf " + dirName + "/" + fileName + "*" ).c_str() ) != 0 ) {
        BOOST_THROW_EXCEPTION( runtime_error( "Remove failed" ) );
    }

    return make_shared< DBType >( sChain, dirName, fileName, node_id( 1 ), 5000000 );
}

template < typename DBType >
ptr< DBType > reopenDB( const string& _dbName ) {
    auto node = createTestNode();
    auto sChain = createTestSchainWithNode( node );
    
    // Keep node alive for the lifetime of this DB
    testNodes[_dbName + "_reopen"] = node;
    
    string dirName = TEST_DB_DIR;
    string fileName = _dbName;

    // Do NOT clean up - reopen existing DB
    return make_shared< DBType >( sChain, dirName, fileName, node_id( 1 ), 5000000 );
}

void cleanupDB( const string& _dbName ) {
    string dirName = TEST_DB_DIR;
    string fileName = _dbName;

    if ( std::system( ( "rm -rf " + dirName + "/" + fileName + "*" ).c_str() ) != 0 ) {
        BOOST_THROW_EXCEPTION( runtime_error( "Cleanup failed" ) );
    }
    
    // Clean up stored nodes
    testNodes.erase( _dbName );
    testNodes.erase( _dbName + "_reopen" );
}

}  // namespace DBTestUtils
