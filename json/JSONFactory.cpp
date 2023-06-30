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

#undef CHECK

#include "BLSPublicKey.h"


#include "sgxwallet/stubclient.h"
#include <jsonrpccpp/client.h>
#include <jsonrpccpp/client/connectors/httpclient.h>
#include <jsonrpccpp/server/connectors/httpserver.h>
#include <libBLS/bls/BLSPublicKeyShare.h>
#include <libBLS/bls/BLSSigShare.h>
#include <libBLS/bls/BLSSigShareSet.h>

#include "Log.h"
#include "SkaleCommon.h"
#include "exceptions/FatalError.h"
#include "thirdparty/json.hpp"

#include "chains/SchainTest.h"
#include "network/Network.h"

#include "chains/Schain.h"


#include "crypto/BLAKE3Hash.h"

#include "exceptions/ParsingException.h"
#include "network/Sockets.h"
#include "node/ConsensusEngine.h"
#include "node/Node.h"
#include "node/NodeInfo.h"

#include "crypto/CryptoManager.h"
#include "JSONFactory.h"


ptr< Node > JSONFactory::createNodeFromTestJsonFile( const string& _sgxUrl, const fs_path& jsonFile,
    set< node_id >& nodeIDs, ConsensusEngine* _consensusEngine, bool _useSGX,
    const string& _sgxSSLKeyFileFullPath, const string& _sgxSSLCertFileFullPath,
    const string& _ecdsaKeyName, const ptr< vector< string > >& _ecdsaPublicKeys,
    const string& _blsKeyName, const ptr< vector< ptr< vector< string > > > >& _blsPublicKeys,
    const ptr< BLSPublicKey >& _blsPublicKey,
    const ptr< map< uint64_t, ptr< BLSPublicKey > > >& _previousBlsPublicKeys,
    const ptr< map< uint64_t, string > >& _historicECDSAPublicKeys,
    const ptr< map< uint64_t, vector< uint64_t > > >& _historicNodeGroups ) {
    string sgxUrl = "";

    try {
        if ( _useSGX ) {
            CHECK_ARGUMENT( !_ecdsaKeyName.empty() )
            CHECK_ARGUMENT( _ecdsaPublicKeys )
            CHECK_ARGUMENT( !_blsKeyName.empty() )
            CHECK_ARGUMENT( _blsPublicKeys )
            CHECK_ARGUMENT( _blsPublicKey )
            sgxUrl = _sgxUrl;
        }

        nlohmann::json j;

        parseJsonFile( j, jsonFile );


        string gethURL = "";

        return createNodeFromJsonObject( j, nodeIDs, _consensusEngine, _useSGX, sgxUrl,
            _sgxSSLKeyFileFullPath, _sgxSSLCertFileFullPath, _ecdsaKeyName, _ecdsaPublicKeys,
            _blsKeyName, _blsPublicKeys, _blsPublicKey, gethURL, _previousBlsPublicKeys,
            _historicECDSAPublicKeys, _historicNodeGroups );
    } catch ( ... ) {
        throw_with_nested( FatalError( __FUNCTION__ + to_string( __LINE__ ), __CLASS_NAME__ ) );
    }
}

ptr< Node > JSONFactory::createNodeFromJsonObject( const nlohmann::json& _j,
    set< node_id >& nodeIDs, ConsensusEngine* _engine, bool _useSGX, const string& _sgxURL,
    const string& _sgxSSLKeyFileFullPath, const string& _sgxSSLCertFileFullPath,
    const string& _ecdsaKeyName, const ptr< vector< string > >& _ecdsaPublicKeys,
    const string& _blsKeyName, const ptr< vector< ptr< vector< string > > > >& _blsPublicKeys,
    const ptr< BLSPublicKey >& _blsPublicKey, string& _gethURL,
    const ptr< map< uint64_t, ptr< BLSPublicKey > > >& _previousBlsPublicKeys,
    const ptr< map< uint64_t, string > >& _historicECDSAPublicKeys,
    const ptr< map< uint64_t, vector< uint64_t > > >& _historicNodeGroups ) {
    bool isSyncNode = false;


    if ( _j.find( "syncNode" ) != _j.end() ) {
        isSyncNode = _j.at( "syncNode" ).get< bool >();
    }


    if ( isSyncNode && _useSGX ) {
        LOG( err, "SGX mode is not available for a sync node" );
        _useSGX = false;
    }


    auto sgxSSLKeyFileFullPathCopy = _sgxSSLKeyFileFullPath;
    auto sgxSSLCertFileFullPathCopy = _sgxSSLCertFileFullPath;


    if ( _useSGX ) {
        if ( _sgxSSLKeyFileFullPath.empty() && ( _j.count( "sgxKeyFileFullPath" ) > 0 ) ) {
            sgxSSLKeyFileFullPathCopy = string( _j.at( "sgxKeyFileFullPath" ).get< string >() );
        }
        if ( _sgxSSLCertFileFullPath.empty() && ( _j.count( "sgxCertFileFullPath" ) > 0 ) ) {
            sgxSSLCertFileFullPathCopy = string( _j.at( "sgxCertFileFullPath" ).get< string >() );
        }

        CHECK_ARGUMENT( !_ecdsaKeyName.empty() && _ecdsaPublicKeys );
        CHECK_ARGUMENT( !_blsKeyName.empty() && _blsPublicKeys );
        CHECK_ARGUMENT( _blsPublicKey );
        CHECK_ARGUMENT( _previousBlsPublicKeys );
        CHECK_ARGUMENT( _historicECDSAPublicKeys );
        CHECK_ARGUMENT( _historicNodeGroups );
        CHECK_STATE( JSONFactory::splitString( _ecdsaKeyName )->size() == 2 );
        CHECK_STATE( JSONFactory::splitString( _blsKeyName )->size() == 7 );
    }

    Network::setTransport( TransportType::ZMQ );

    if ( _j.find( "logLevelConfig" ) != _j.end() ) {
        string logLevel( _j.at( "logLevelConfig" ).get< string >() );
        _engine->setConfigLogLevel( logLevel );
    }

    uint64_t nodeID = _j.at( "nodeID" ).get< uint64_t >();

    ptr< Node > node = nullptr;

    if ( nodeIDs.empty() || nodeIDs.count( node_id( nodeID ) ) > 0 ) {
        try {
            node = make_shared< Node >( _j, _engine, _useSGX, _sgxURL, sgxSSLKeyFileFullPathCopy,
                sgxSSLCertFileFullPathCopy, _ecdsaKeyName, _ecdsaPublicKeys, _blsKeyName,
                _blsPublicKeys, _blsPublicKey, _gethURL, _previousBlsPublicKeys,
                _historicECDSAPublicKeys, _historicNodeGroups, isSyncNode );
        } catch ( ... ) {
            throw_with_nested( FatalError( "Could not init node", __CLASS_NAME__ ) );
        }
    }

    return node;
}

void JSONFactory::createAndAddSChainFromJson(
    const ptr< Node >& _node, const fs_path& _jsonFile, ConsensusEngine* _engine ) {
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
            } else if ( test == SchainTest::BAD_NETWORK ) {
                _node->getSchain()->setBlockProposerTest( SchainTest::BAD_NETWORK );
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
    const ptr< Node >& _node, const nlohmann::json& j, ConsensusEngine* _engine ) {
    nlohmann::json element;

    try {
        if ( j.count( "skaleConfig" ) > 0 ) {
            element = j.at( "skaleConfig" );
            try {
                element = element.at( "nodeInfo" );
            } catch ( ... ) {
                BOOST_THROW_EXCEPTION( InvalidStateException(
                    "Couldnt find nodeInfo element in skaleConfig", __CLASS_NAME__ ) );
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
                InvalidStateException( "Couldnt find schainix in json config", __CLASS_NAME__ ) );
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

            string ip( ( *it ).at( "ip" ).get< string >() );

            network_port port;

            try {
                port = it->at( "basePort" ).get< int >();
            } catch ( ... ) {
                BOOST_THROW_EXCEPTION( InvalidStateException(
                    "Couldnt find basePort in json config", __CLASS_NAME__ ) );
            }


            schain_index schainIndex;

            try {
                schainIndex = it->at( "schainIndex" ).get< uint64_t >();
            } catch ( ... ) {
                BOOST_THROW_EXCEPTION( InvalidStateException(
                    "Couldnt find schainIndex in json config", __CLASS_NAME__ ) );
            }

            auto rni = make_shared< NodeInfo >( nodeID, ip, port, schainID, schainIndex );

            if ( nodeID == _node->getNodeID() )
                localNodeInfo = rni;

            remoteNodeInfos.push_back( rni );
        }

        schain_index schainIndex = 0;

        if ( !_node->isSyncOnlyNode() ) {
            // node readonly node should be part of schain
            CHECK_STATE( localNodeInfo );
            schainIndex = localNodeInfo->getSchainIndex();
        }


        Node::initSchain(
            _node, schainIndex, schainID, remoteNodeInfos, _engine->getExtFace(), schainName );

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


using namespace jsonrpc;


tuple< ptr< vector< string > >, ptr< vector< string > >, ptr< vector< string > >,
    ptr< vector< ptr< vector< string > > > >, ptr< BLSPublicKey > >
JSONFactory::parseTestKeyNamesFromJson( const string& _sgxServerURL, const fs_path& configFile,
    uint64_t _totalNodes, uint64_t _requiredNodes ) {
    CHECK_ARGUMENT( _totalNodes > 0 );
    CHECK_STATE( _totalNodes >= _requiredNodes );

    auto ecdsaKeyNames = make_shared< vector< string > >();
    auto ecdsaPublicKeys = make_shared< vector< string > >();
    auto blsKeyNames = make_shared< vector< string > >();
    auto blsPublicKeys = make_shared< vector< ptr< vector< string > > > >();


    nlohmann::json j;

    parseJsonFile( j, configFile );


    auto ecdsaKeyNamesObject = j.at( "ecdsaKeyNames" );
    auto blsKeyNamesObject = j.at( "blsKeyNames" );

    CHECK_STATE( ecdsaKeyNamesObject.is_object() );
    CHECK_STATE( blsKeyNamesObject.is_object() );


    CHECK_STATE( ecdsaKeyNamesObject.size() == _totalNodes );
    CHECK_STATE( blsKeyNamesObject.size() == _totalNodes );

    for ( uint64_t i = 1; i <= _totalNodes; i++ ) {
        auto key = to_string( i );
        string fullKey( 3 - key.size(), '0' );
        fullKey.append( key );

        auto ecdsaKeyName = ecdsaKeyNamesObject.at( fullKey );
        CHECK_STATE( JSONFactory::splitString( ecdsaKeyName )->size() == 2 );
        ecdsaKeyNames->push_back( ecdsaKeyName );

        auto blsKeyName = blsKeyNamesObject.at( fullKey );


        CHECK_STATE( JSONFactory::splitString( blsKeyName )->size() == 7 );


        blsKeyNames->push_back( blsKeyName );
    }


    CHECK_STATE( ecdsaKeyNames->size() == _totalNodes );
    CHECK_STATE( blsKeyNames->size() == _totalNodes );

    HttpClient client( _sgxServerURL );
    StubClient c( client, JSONRPC_CLIENT_V2 );

    LOG( info, "Getting BLS Public Key Shares." );

    for ( uint64_t i = 0; i < _totalNodes; i++ ) {
        Json::Value response;


        RETRY_BEGIN
        response = c.getBLSPublicKeyShare( blsKeyNames->at( i ) );
        RETRY_END

        JSONFactory::checkSGXStatus( response );

        auto fourPieces = response["blsPublicKeyShare"];
        CHECK_STATE( fourPieces );
        CHECK_STATE( fourPieces.isArray() );
        CHECK_STATE( fourPieces.size() == 4 );

        blsPublicKeys->push_back( make_shared< vector< string > >() );

        for ( uint64_t k = 0; k < 4; k++ ) {
            auto element = fourPieces[( int ) k];
            CHECK_STATE( element )
            CHECK_STATE( element.isString() )

            string keyPiece;
            keyPiece += element.asString();

            CHECK_STATE( strlen( keyPiece.c_str() ) == keyPiece.size() );
            CHECK_STATE( keyPiece.size() > 0 );

            blsPublicKeys->back()->push_back( keyPiece );
        }
    }


    // create pub key

    auto blsPublicKeysMap = make_shared< map< size_t, ptr< BLSPublicKeyShare > > >();

    for ( uint64_t i = 0; i < _requiredNodes; i++ ) {
        LOG( info, "Configured BLS public key share for node index "
                       << to_string( i + 1 ) << ":" << blsPublicKeys->at( i )->at( 0 ) << ":"
                       << blsPublicKeys->at( i )->at( 1 ) << ":" << blsPublicKeys->at( i )->at( 2 )
                       << ":" << blsPublicKeys->at( i )->at( 3 ) );

        auto share =
            make_shared< BLSPublicKeyShare >( blsPublicKeys->at( i ), _requiredNodes, _totalNodes );

        CHECK_STATE( share->getPublicKey() );

        blsPublicKeysMap->insert( std::pair< size_t, ptr< BLSPublicKeyShare > >( i + 1, share ) );
    }


    LOG( info, "Computing BLS public key" );

    auto blsPublicKey =
        make_shared< BLSPublicKey >( blsPublicKeysMap, _requiredNodes, _totalNodes );

    LOG( info, "Computed BLS Public Key" );

    LOG( info, "Verifying a sample sig" );


    // sign verify a sample sig

    vector< Json::Value > blsSigShares( _totalNodes );
    BLSSigShareSet sigShareSet( _requiredNodes, _totalNodes );

    string SAMPLE_HASH( "09c6137b97cdf159b9950f1492ee059d1e2b10eaf7d51f3a97d61f2eee2e81db" );

    auto hash = BLAKE3Hash::fromHex( SAMPLE_HASH );

    for ( uint64_t i = 0; i < _requiredNodes; i++ ) {
        {
            RETRY_BEGIN

            blsSigShares.at( i ) = c.blsSignMessageHash(
                blsKeyNames->at( i ), SAMPLE_HASH, _requiredNodes, _totalNodes );
            RETRY_END
        }

        JSONFactory::checkSGXStatus( blsSigShares[i] );


        string sigShareStr;
        sigShareStr += getString( blsSigShares[i], "signatureShare" );
        auto sigShare = make_shared< string >( sigShareStr );

        BLSSigShare sig( sigShare, i + 1, _requiredNodes, _totalNodes );
        sigShareSet.addSigShare( make_shared< BLSSigShare >( sig ) );

        auto pubKey = blsPublicKeysMap->at( i + 1 );

        auto sharedHash = make_shared< array< uint8_t, HASH_LEN > >( hash.getHash() );

        CHECK_STATE( pubKey->VerifySigWithHelper(
            sharedHash, make_shared< BLSSigShare >( sig ), _requiredNodes, _totalNodes ) );
    }

    ptr< BLSSignature > commonSig = sigShareSet.merge();

    auto sharedHash = make_shared< array< uint8_t, HASH_LEN > >( hash.getHash() );

    CHECK_STATE( blsPublicKey->VerifySigWithHelper( sharedHash, commonSig ) );

    LOG( info, "Verified a sample sig" );

    LOG( info, "Getting ECDSA keys" );

    for ( uint64_t i = 0; i < _totalNodes; i++ ) {
        LOG( info,
            "Getting ECDSA public key for " << ecdsaKeyNames->at( i ).substr( 0, 8 ) << "..." );

        auto response = c.getPublicECDSAKey( ecdsaKeyNames->at( i ) );

        JSONFactory::checkSGXStatus( response );

        auto publicKey = getString( response, "publicKey" );

        LOG( info, "Got ECDSA public key:" << publicKey );

        ecdsaPublicKeys->push_back( publicKey );
    }


    CHECK_STATE( ecdsaKeyNames->size() == _totalNodes )
    CHECK_STATE( blsKeyNames->size() == _totalNodes )
    CHECK_STATE( ecdsaPublicKeys->size() == _totalNodes )
    CHECK_STATE( blsPublicKeys->size() == _totalNodes );

    LOG( info, "Got ECDSA keys" );

    return { ecdsaKeyNames, ecdsaPublicKeys, blsKeyNames, blsPublicKeys, blsPublicKey };
}


ptr< vector< string > > JSONFactory::splitString( const string& _str, const string& _delim ) {
    auto tokens = make_shared< vector< string > >();
    size_t prev = 0, pos;
    do {
        pos = _str.find( _delim, prev );
        if ( pos == string::npos )
            pos = _str.length();
        string token = _str.substr( prev, pos - prev );
        if ( !token.empty() )
            tokens->push_back( token );
        prev = pos + _delim.length();
    } while ( pos < _str.length() && prev < _str.length() );

    return tokens;
}


void JSONFactory::checkSGXStatus( Json::Value& _result ) {
    auto status = JSONFactory::getInt64( _result, "status" );

    if ( status != 0 ) {
        LOG( err, "SGX server returned error status:" << to_string( status ) );
        LOG( err, _result.asString() );
    }

    CHECK_STATE( status == 0 );
}

int64_t JSONFactory::getInt64( Json::Value& _json, const char* key ) {
    CHECK_STATE( _json );
    CHECK_STATE( key );
    auto value = _json[key];
    CHECK_STATE( value && value.isInt64() )
    return value.asInt64();
}

string JSONFactory::getString( Json::Value& _json, const char* key ) {
    auto value = _json[key];
    CHECK_STATE( value && value.isString() )
    return value.asString();
}
