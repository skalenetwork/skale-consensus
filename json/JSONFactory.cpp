/*
    Copyright (C) 2018-2019 SKALE Labs

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

    @file JSONFactory.cpp
    @author Stan Kladko
    @date 2018
*/

#include "thirdparty/catch.hpp"

#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"
#include "thirdparty/json.hpp"

#include "chains/SchainTest.h"
#include "network/Network.h"

#include "chains/Schain.h"

#include "Log.h"
#include "exceptions/FatalError.h"
#include "exceptions/ParsingException.h"
#include "network/Sockets.h"
#include "node/ConsensusEngine.h"
#include "node/Node.h"
#include "node/NodeInfo.h"

#include "JSONFactory.h"

ptr< Node > JSONFactory::createNodeFromJson( const fs_path& jsonFile, set< node_id >& nodeIDs,
    ConsensusEngine* _consensusEngine, bool _useSGX, ptr< string > _keyName,
    ptr< vector< string > > _publicKeys ) {
    try {
        if ( _useSGX ) {
            CHECK_ARGUMENT( _keyName );
            CHECK_ARGUMENT( _publicKeys )
        }

        nlohmann::json j;

        parseJsonFile( j, jsonFile );

        return createNodeFromJsonObject(
            j, nodeIDs, _consensusEngine, _useSGX, _keyName, _publicKeys );
    } catch ( ... ) {
        throw_with_nested( FatalError( __FUNCTION__ + to_string( __LINE__ ), __CLASS_NAME__ ) );
    }
}

ptr< Node > JSONFactory::createNodeFromJsonObject( const nlohmann::json& j, set< node_id >& nodeIDs,
    ConsensusEngine* _engine, bool _useSGX, ptr< string > _keyName,
    ptr< vector< string > > _publicKeys ) {
    if ( _useSGX ) {
        CHECK_ARGUMENT( _keyName && _publicKeys );
    }

    if ( j.find( "transport" ) != j.end() ) {
        ptr< string > transport = make_shared< string >( j.at( "transport" ).get< string >() );
        Network::setTransport( TransportType::ZMQ );
    }


    if ( j.find( "logLevelConfig" ) != j.end() ) {
        ptr< string > logLevel = make_shared< string >( j.at( "logLevelConfig" ).get< string >() );
        _engine->setConfigLogLevel( *logLevel );
    }

    uint64_t nodeID = j.at( "nodeID" ).get< uint64_t >();

    ptr< Node > node = nullptr;

    if ( nodeIDs.empty() || nodeIDs.count( node_id( nodeID ) ) > 0 ) {
        try {
            node = make_shared< Node >( j, _engine, _useSGX, _keyName, _publicKeys );
        } catch ( ... ) {
            throw_with_nested( FatalError( "Could not init node", __CLASS_NAME__ ) );
        }
    }

    return node;
}

void JSONFactory::createAndAddSChainFromJson(
    ptr< Node > _node, const fs_path& _jsonFile, ConsensusEngine* _engine ) {
    try {
        nlohmann::json j;


        _engine->logConfig( debug, "Parsing json file: " + _jsonFile.string(), __CLASS_NAME__ );


        parseJsonFile( j, _jsonFile );

        createAndAddSChainFromJsonObject( _node, j, _engine );

        if ( j.find( "blockProposalTest" ) != j.end() ) {
            string test = j["blockProposalTest"].get< string >();

            if ( test == SchainTest::NONE ) {
                _node->getSchain()->setBlockProposerTest( SchainTest::NONE );
            } else if ( test == SchainTest::SLOW ) {
                _node->getSchain()->setBlockProposerTest( SchainTest::SLOW );
            } else {
                BOOST_THROW_EXCEPTION( ParsingException(
                    "Unknown test type parsing schain config:" + test, __CLASS_NAME__ ) );
            }
        }
    } catch ( ... ) {
        throw_with_nested( FatalError( __FUNCTION__, __CLASS_NAME__ ) );
    }
}

void JSONFactory::createAndAddSChainFromJsonObject(
    ptr< Node >& _node, const nlohmann::json& j, ConsensusEngine* _engine ) {
    nlohmann::json element;

    try {

        if ( j.count( "skaleConfig" ) > 0 ) {
            element = j.at( "skaleConfig" );
            try {
                element = element.at( "nodeInfo" );
            } catch ( ... ) {
                BOOST_THROW_EXCEPTION(
                    InvalidStateException( "Couldnt find nodeInfo element in skaleConfig", __CLASS_NAME__ ) );
            }
        } else {
            element = j;
        }

        string schainName;
        schain_id schainID;

        try {
            schainName = element.at( "schainName" ).get< string >();
        } catch ( ... ) {
            BOOST_THROW_EXCEPTION(
                InvalidStateException( "Couldnt find schainName in json config", __CLASS_NAME__ ) );
        }

        try {
            schainID = element.at( "schainID" ).get< uint64_t >();
        } catch ( ... ) {
            BOOST_THROW_EXCEPTION(
                InvalidStateException( "Couldnt find schainName in json config", __CLASS_NAME__ ) );
        }

        uint64_t emptyBlockIntervalMs;
        int64_t emptyBlockIntervalMsTmp;

        try {
            emptyBlockIntervalMsTmp = element.at( "emptyBlockIntervalMs" ).get< int64_t >();
        } catch ( ... ) {
            emptyBlockIntervalMsTmp = EMPTY_BLOCK_INTERVAL_MS;
        }

        if ( emptyBlockIntervalMsTmp < 0 ) {
            emptyBlockIntervalMs = 100000000000000;
        } else {
            emptyBlockIntervalMs = emptyBlockIntervalMsTmp;
        }

        _node->setEmptyBlockIntervalMs( emptyBlockIntervalMs );

        ptr< NodeInfo > localNodeInfo = nullptr;

        vector< ptr< NodeInfo > > remoteNodeInfos;

        nlohmann::json nodes;

        try {
            nodes = element.at( "nodes" );
        } catch ( ... ) {
            BOOST_THROW_EXCEPTION(
                InvalidStateException( "Couldnt find nodes in json config", __CLASS_NAME__ ) );
        }

        for ( auto it = nodes.begin(); it != nodes.end(); ++it ) {
            node_id nodeID;

            try {
                nodeID = it->at( "nodeID" ).get< uint64_t >();
            } catch ( ... ) {
                BOOST_THROW_EXCEPTION(
                    InvalidStateException( "Couldnt find nodeID in json config", __CLASS_NAME__ ) );
            }

            _engine->logConfig( trace,
                to_string( _node->getNodeID() ) + ": Adding node:" + to_string( nodeID ),
                __CLASS_NAME__ );

            ptr< string > ip = make_shared< string >( ( *it ).at( "ip" ).get< string >() );

            network_port port;

            try {
                port = it ->at( "basePort" ).get< int >();
            } catch ( ... ) {
                BOOST_THROW_EXCEPTION(
                    InvalidStateException( "Couldnt find basePort in json config", __CLASS_NAME__ ) );
            }


            schain_index schainIndex;

            try {
                schainIndex = it->at( "schainIndex" ).get< uint64_t >();
            } catch ( ... ) {
                BOOST_THROW_EXCEPTION(
                    InvalidStateException( "Couldnt find schainIndex in json config", __CLASS_NAME__ ) );
            }

            auto rni = make_shared< NodeInfo >( nodeID, ip, port, schainID, schainIndex );

            if ( nodeID == _node->getNodeID() )
                localNodeInfo = rni;

            remoteNodeInfos.push_back( rni );
        }

        ASSERT( localNodeInfo );
        Node::initSchain( _node, localNodeInfo, remoteNodeInfos, _engine->getExtFace() );
    } catch ( ... ) {
        throw_with_nested( FatalError( __FUNCTION__, __CLASS_NAME__ ) );
    }
}

void JSONFactory::parseJsonFile( nlohmann::json& j, const fs_path& configFile ) {
    ifstream f( configFile.c_str() );

    if ( f.good() ) {
        f >> j;
    } else {
        BOOST_THROW_EXCEPTION( FatalError(
            "Could not find config file: JSON file does not exist " + configFile.string(),
            __CLASS_NAME__ ) );
    }
}
pair< ptr< vector< string > >, ptr< vector< string > > > JSONFactory::parseTestKeyNamesFromJson(
    const fs_path& configFile, uint64_t _totalNodes) {

    auto ecdsaKeyNames = make_shared<vector<string>>();
    auto blsKeyNames = make_shared<vector<string>>();

    nlohmann::json j;

    parseJsonFile(j, configFile);


    auto ecdsaKeyNamesObject = j.at("ecdsaKeyNames");
    auto blsKeyNamesObject = j.at("blsKeyNames");

    CHECK_STATE( ecdsaKeyNamesObject.is_object());
    CHECK_STATE( blsKeyNamesObject.is_object());


    CHECK_STATE( ecdsaKeyNamesObject.size() == _totalNodes);
    CHECK_STATE( blsKeyNamesObject.size() == _totalNodes);

    for (uint64_t i = 1; i <= _totalNodes; i++ ) {
        auto ecdsaKeyName = ecdsaKeyNamesObject.at(to_string(i));
        ecdsaKeyNames->push_back( ecdsaKeyName );

        auto blsKeyName = blsKeyNamesObject.at(to_string(i));
        blsKeyNames->push_back( blsKeyName );
    }


    CHECK_STATE(ecdsaKeyNames->size() == _totalNodes );
    CHECK_STATE(blsKeyNames->size() == _totalNodes );

    return {ecdsaKeyNames, blsKeyNames};
}

