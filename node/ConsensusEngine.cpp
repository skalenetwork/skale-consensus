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

   @file ConsensusEngine.cpp
   @author Stan Kladko
   @date 2018
*/


#include "openssl/err.h"
#include "openssl/evp.h"
#include "openssl/pem.h"

#include "Agent.h"
#include "Log.h"
#include "SkaleCommon.h"
#include "exceptions/FatalError.h"
#include "thirdparty/json.hpp"
#include "threads/GlobalThreadRegistry.h"

#include "zmq.h"


#pragma GCC diagnostic push
// Suppress warnings: "unknown option after ‘#pragma GCC diagnostic’ kind [-Wpragmas]".
// This is necessary because not all the compilers have the same warning options.
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wdeprecated-register"
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wmismatched-tags"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wtautological-compare"
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunneeded-internal-declaration"
#pragma GCC diagnostic ignored "-Wunused-private-field"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wignored-attributes"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#pragma GCC diagnostic ignored "-Wchar-subscripts"
#pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Wreorder"

#include "bls.h"

#pragma GCC diagnostic pop

#include <libBLS/bls/BLSPublicKeyShare.h>
#include <boost/multiprecision/cpp_int.hpp>


#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include "chains/Schain.h"
#include "exceptions/EngineInitException.h"
#include "json/JSONFactory.h"
#include "libBLS/bls/BLSPrivateKeyShare.h"
#include "libBLS/bls/BLSPublicKey.h"
#include "libBLS/bls/BLSSignature.h"
#include "network/Sockets.h"
#include "network/Utils.h"
#include "network/ZMQSockets.h"
#include "protocols/ProtocolKey.h"
#include "protocols/binconsensus/BinConsensusInstance.h"


#include "exceptions/FatalError.h"

#include "ConsensusEngine.h"

#include "db/StorageLimits.h"

#include "ENGINE_VERSION"

using namespace boost::filesystem;


shared_ptr< spdlog::logger > ConsensusEngine::configLogger = nullptr;

shared_ptr< string > ConsensusEngine::dataDir = nullptr;

recursive_mutex ConsensusEngine::logMutex;

atomic< uint64_t > ConsensusEngine::engineCounter;


void ConsensusEngine::logInit() {
    engineID = ++engineCounter;

    LOCK( logMutex )

    spdlog::flush_every( std::chrono::seconds( 1 ) );

    logThreadLocal_ = nullptr;


    if ( dataDir == nullptr ) {
        char* d = std::getenv( "DATA_DIR" );

        if ( d != nullptr ) {
            dataDir = make_shared< string >( d );
            cerr << "Found data dir:" << *dataDir << endl;
        }
    }


    string logFileName;
    if ( engineID > 1 ) {
        logFileName = "skaled." + to_string( engineID ) + ".log";
    } else {
        logFileName = "skaled.log";
    }

    if ( dataDir != nullptr ) {
        logFileNamePrefix = make_shared< string >( *dataDir + "/" + logFileName );
        logRotatingFileSync = make_shared< spdlog::sinks::rotating_file_sink_mt >(
            *logFileNamePrefix, 10 * 1024 * 1024, 5 );
        healthCheckDir = dataDir;
        dbDir = dataDir;
    } else {
        healthCheckDir = make_shared< string >( "/tmp" );
        dbDir = healthCheckDir;
        logFileNamePrefix = nullptr;
        logRotatingFileSync = nullptr;
    }


    configLogger = createLogger( "config" );
}


const shared_ptr< string > ConsensusEngine::getDataDir() {
    CHECK_STATE( dataDir );
    return dataDir;
}


shared_ptr< spdlog::logger > ConsensusEngine::createLogger( const string& loggerName ) {
    shared_ptr< spdlog::logger > logger = spdlog::get( loggerName );

    if ( !logger ) {
        if ( logFileNamePrefix != nullptr ) {
            logger = make_shared< spdlog::logger >( loggerName, logRotatingFileSync );
            logger->flush_on( debug );
        } else {
            logger = spdlog::stdout_color_mt( loggerName );
        }
        logger->set_pattern( "%+", spdlog::pattern_time_type::utc );
    }

    CHECK_STATE( logger );

    return logger;
}


void ConsensusEngine::setConfigLogLevel( string& _s ) {
    auto configLogLevel = SkaleLog::logLevelFromString( _s );
    CHECK_STATE( configLogger != nullptr );
    configLogger->set_level( configLogLevel );
}

void ConsensusEngine::logConfig(
    level_enum _severity, const string& _message, const string& _className ) {
    CHECK_STATE( configLogger != nullptr );
    configLogger->log( _severity, _className + ": " + _message );
}

void ConsensusEngine::log(
    level_enum _severity, const string& _message, const string& _className ) {
    if ( logThreadLocal_ == nullptr ) {
        CHECK_STATE( configLogger != nullptr );
        configLogger->log( _severity, _message );
    } else {
        auto engine = logThreadLocal_->getEngine();
        CHECK_STATE( engine );

        string fullMessage =
            to_string( ( uint64_t ) engine->getLargestCommittedBlockID() ) + ":" + _message;

        logThreadLocal_->loggerForClass( _className.c_str() )->log( _severity, fullMessage );
    }
}


void ConsensusEngine::parseFullConfigAndCreateNode( const string& configFileContents ) {
    try {
        nlohmann::json j = nlohmann::json::parse( configFileContents );

        std::set< node_id > dummy;

        ptr< Node > node = nullptr;

        if ( this->isSGXEnabled ) {
            node = JSONFactory::createNodeFromJsonObject( j["skaleConfig"]["nodeInfo"], dummy, this,
                true, sgxServerUrl, sgxSSLKeyFileFullPath, sgxSSLCertFileFullPath,
                getEcdsaKeyName(), ecdsaPublicKeys, getBlsKeyName(), blsPublicKeys, blsPublicKey );
        } else {
            node = JSONFactory::createNodeFromJsonObject( j["skaleConfig"]["nodeInfo"], dummy, this,
                false, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr );
        }

        JSONFactory::createAndAddSChainFromJsonObject( node, j["skaleConfig"]["sChain"], this );

        nodes[node->getNodeID()] = node;

    } catch ( SkaleException& e ) {
        SkaleException::logNested( e );
        throw;
    }
}

ptr< Node > ConsensusEngine::readNodeConfigFileAndCreateNode( const fs_path& path,
    set< node_id >& _nodeIDs, bool _useSGX, ptr< string > _sgxSSLKeyFileFullPath,
    ptr< string > _sgxSSLCertFileFullPath, ptr< string > _ecdsaKeyName,
    ptr< vector< string > > _ecdsaPublicKeys, ptr< string > _blsKeyName,
    ptr< vector< ptr< vector< string > > > > _blsPublicKeys,
    ptr< vector< string > > _blsPublicKey ) {
    try {
        if ( _useSGX ) {
            CHECK_ARGUMENT( _ecdsaKeyName && _ecdsaPublicKeys );
            CHECK_ARGUMENT( _blsKeyName && _blsPublicKeys );
            CHECK_ARGUMENT( _blsPublicKey && _blsPublicKey->size() == 4 );
        }


        fs_path nodeFileNamePath( path );

        nodeFileNamePath /= string( SkaleCommon::NODE_FILE_NAME );

        checkExistsAndFile( nodeFileNamePath.string() );

        fs_path schainDirNamePath( path );

        schainDirNamePath /= string( SkaleCommon::SCHAIN_DIR_NAME );

        checkExistsAndDirectory( schainDirNamePath.string() );


        auto node = JSONFactory::createNodeFromJsonFile( sgxServerUrl, nodeFileNamePath.string(),
            _nodeIDs, this, _useSGX, _sgxSSLKeyFileFullPath, _sgxSSLCertFileFullPath, _ecdsaKeyName,
            _ecdsaPublicKeys, _blsKeyName, _blsPublicKeys, _blsPublicKey );


        if ( node == nullptr ) {
            return nullptr;
        }

        readSchainConfigFiles( node, schainDirNamePath.string() );

        ASSERT( nodes.count( node->getNodeID() ) == 0 );

        nodes[node->getNodeID()] = node;
        return node;

    } catch ( SkaleException& e ) {
        SkaleException::logNested( e );
        throw;
    }
}


void ConsensusEngine::readSchainConfigFiles( ptr< Node > _node, const fs_path& _dirPath ) {
    try {
        checkExistsAndDirectory( _dirPath );

        directory_iterator itr( _dirPath );

        auto end = directory_iterator();


        // cycle through the directory

        for ( ; itr != end; ++itr ) {
            fs_path jsonFile = itr->path();
            LOG( debug, "Parsing file:" + jsonFile.string() );
            JSONFactory::createAndAddSChainFromJson( _node, jsonFile, this );
            break;
        }

    } catch ( ... ) {
        throw_with_nested( FatalError( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


void ConsensusEngine::checkExistsAndDirectory( const fs_path& _dirPath ) {
    if ( !exists( _dirPath ) ) {
        BOOST_THROW_EXCEPTION( FatalError( "Not found: " + _dirPath.string() ) );
    }

    if ( !is_directory( _dirPath ) ) {
        BOOST_THROW_EXCEPTION( FatalError( "Not a directory: " + _dirPath.string() ) );
    }
}


void ConsensusEngine::checkExistsAndFile( const fs_path& _filePath ) {
    if ( !exists( _filePath ) ) {
        BOOST_THROW_EXCEPTION( FatalError( "Not found: " + _filePath.string() ) );
    }

    if ( is_directory( _filePath ) ) {
        BOOST_THROW_EXCEPTION(
            FatalError( "Path is a direcotry, regular file is required " + _filePath.string() ) );
    }
}


void ConsensusEngine::parseTestConfigsAndCreateAllNodes( const fs_path& dirname ) {
    try {
        checkExistsAndDirectory( dirname );


        // cycle through the directory

        uint64_t nodeCount = 0;

        directory_iterator itr( dirname );

        auto end = directory_iterator();


        for ( ; itr != end; itr++ ) {
            if ( !is_directory( itr->path() ) ) {
                BOOST_THROW_EXCEPTION(
                    FatalError( "Junk file found. Remove it: " + itr->path().string() ) );
            }

            if ( itr->path().filename().string().find( "corrupt" ) == string::npos )
                nodeCount++;
        };


        bool sgxDirExists = is_directory( dirname.string() + "/../../run_sgx_test/sgx_data" );

        string filePath;

        if ( sgxDirExists ) {
            filePath = dirname.string() + "/../../run_sgx_test/sgx_data" + "/" +
                       to_string( nodeCount ) + "node.json";
            if ( is_regular_file( filePath ) ) {
                CHECK_STATE( nodeCount % 3 == 1 );
                sgxServerUrl = make_shared< string >( "http://localhost:1029" );
                this->setTestKeys( sgxServerUrl, filePath, nodeCount, nodeCount - 1 / 3 );
            }
        }

        if ( isSGXEnabled ) {
            CHECK_STATE( ecdsaPublicKeys );
            CHECK_STATE( ecdsaKeyNames );
            CHECK_STATE( blsKeyNames );
            CHECK_STATE( blsPublicKeys );
            CHECK_STATE( blsPublicKey );
            CHECK_STATE( nodeCount == ecdsaPublicKeys->size() );
        }

        directory_iterator itr2( dirname );

        uint64_t i = 0;

        for ( ; itr2 != end; itr2++ ) {
            if ( !is_directory( itr2->path() ) ) {
                BOOST_THROW_EXCEPTION(
                    FatalError( "Junk file found. Remove it: " + itr2->path().string() ) );
            }

            if ( itr2->path().filename().string().find( "corrupt" ) != string::npos ) {
                continue;
            }

            ptr< string > ecdsaKey = nullptr;
            ptr< string > blsKey = nullptr;
            if ( isSGXEnabled ) {
                CHECK_STATE( i < ecdsaKeyNames->size() );
                CHECK_STATE( i < blsKeyNames->size() );
                ecdsaKey = make_shared< string >( ecdsaKeyNames->at( i ) );
                blsKey = make_shared< string >( blsKeyNames->at( i ) );
            }


            // cert and key file name for tests come from the config
            readNodeConfigFileAndCreateNode( itr2->path(), nodeIDs, isSGXEnabled, nullptr, nullptr,
                ecdsaKey, ecdsaPublicKeys, blsKey, blsPublicKeys, blsPublicKey );

            i++;
        };

        if ( nodes.size() == 0 ) {
            BOOST_THROW_EXCEPTION( FatalError( "No valid node dirs found" ) );
        }

        ASSERT( nodeCount == nodes.size() );

        BinConsensusInstance::initHistory( nodes.begin()->second->getSchain()->getNodeCount() );

        LOG( trace,
            "Parsed configs and created " + to_string( ConsensusEngine::nodesCount() ) + " nodes" );
    } catch ( exception& e ) {
        SkaleException::logNested( e );
        throw;
    }
}

void ConsensusEngine::startAll() {
    try {
        for ( auto const it : nodes ) {
            it.second->startServers();
            LOG( info, "Started servers" + to_string( it.second->getNodeID() ) );
        }


        for ( auto const it : nodes ) {
            it.second->startClients();
            LOG( info, "Started clients" + to_string( it.second->getNodeID() ) );
        }

        LOG( info, "Started all nodes" );
    }

    catch ( SkaleException& e ) {
        SkaleException::logNested( e );

        for ( auto const it : nodes ) {
            if ( !it.second->isExitRequested() ) {
                it.second->exitOnFatalError( e.getMessage() );
            }
        }

        spdlog::shutdown();

        throw_with_nested( EngineInitException( "Start all failed", __CLASS_NAME__ ) );
    }
}

void ConsensusEngine::slowStartBootStrapTest() {
    for ( auto const it : nodes ) {
        it.second->startServers();
    }

    for ( auto const it : nodes ) {
        it.second->startClients();
        it.second->getSchain()->bootstrap( lastCommittedBlockID, lastCommittedBlockTimeStamp );
    }

    LOG( info, "Started all nodes" );
}

void ConsensusEngine::bootStrapAll() {
    try {
        for ( auto const it : nodes ) {
            LOG( trace, "Bootstrapping node" );
            it.second->getSchain()->bootstrap( lastCommittedBlockID, lastCommittedBlockTimeStamp );
            LOG( trace, "Bootstrapped node" );
        }
    } catch ( exception& e ) {
        for ( auto const it : nodes ) {
            if ( !it.second->isExitRequested() ) {
                it.second->exitOnFatalError( e.what() );
            }
        }

        spdlog::shutdown();

        SkaleException::logNested( e );

        throw_with_nested(
            EngineInitException( "Consensus engine bootstrap failed", __CLASS_NAME__ ) );
    }
}


node_count ConsensusEngine::nodesCount() {
    return node_count( nodes.size() );
}


std::string ConsensusEngine::exec( const char* cmd ) {
    std::array< char, 128 > buffer;
    std::string result;
    std::unique_ptr< FILE, decltype( &pclose ) > pipe( popen( cmd, "r" ), pclose );
    if ( !pipe ) {
        BOOST_THROW_EXCEPTION( std::runtime_error( "popen() failed!" ) );
    }
    while ( fgets( buffer.data(), buffer.size(), pipe.get() ) != nullptr ) {
        result += buffer.data();
    }
    return result;
}

void ConsensusEngine::systemHealthCheck() {
    string ulimit;
    try {
        ulimit = exec( "/bin/bash -c \"ulimit -n\"" );
    } catch ( ... ) {
        const char* errStr = "Execution of /bin/bash -c ulimit -n failed";
        throw_with_nested( EngineInitException( errStr, __CLASS_NAME__ ) );
    }
    int noFiles = std::strtol( ulimit.c_str(), NULL, 10 );

    noUlimitCheck = std::getenv( "NO_ULIMIT_CHECK" ) != nullptr;
    onTravis = std::getenv( "CI_BUILD" ) != nullptr;

    if ( noFiles < 65535 && !noUlimitCheck && !onTravis ) {
        const char* error =
            "File descriptor limit (ulimit -n) is less than 65535. Set it to 65535 or more as "
            "described"
            " in https://bugs.launchpad.net/ubuntu/+source/lightdm/+bug/1627769\n";
        cerr << error;
        BOOST_THROW_EXCEPTION( EngineInitException( error, __CLASS_NAME__ ) );
    }

    Utils::checkTime();
}

void ConsensusEngine::init() {
    libff::init_alt_bn128_params();

    threadRegistry = make_shared< GlobalThreadRegistry >();
    logInit();


    sigset_t sigpipe_mask;
    sigemptyset( &sigpipe_mask );
    sigaddset( &sigpipe_mask, SIGPIPE );
    sigset_t saved_mask;
    if ( pthread_sigmask( SIG_BLOCK, &sigpipe_mask, &saved_mask ) == -1 ) {
        BOOST_THROW_EXCEPTION( FatalError( "Could not block SIGPIPE" ) );
    }

    systemHealthCheck();
}


ConsensusEngine::ConsensusEngine( block_id _lastId ) : exitRequested( false ) {
    try {
        init();
        lastCommittedBlockID = _lastId;

        storageLimits = make_shared<StorageLimits>( DEFAULT_DB_STORAGE_UNIT );


    } catch ( exception& e ) {
        SkaleException::logNested( e );
        throw_with_nested( EngineInitException( "Engine construction failed", __CLASS_NAME__ ) );
    }
}

ConsensusEngine::ConsensusEngine( ConsensusExtFace& _extFace, uint64_t _lastCommittedBlockID,
    uint64_t _lastCommittedBlockTimeStamp )
    : exitRequested( false ) {
    try {
        init();

        ASSERT( _lastCommittedBlockTimeStamp < ( uint64_t ) 2 * MODERN_TIME );


        extFace = &_extFace;
        lastCommittedBlockID = block_id( _lastCommittedBlockID );

        ASSERT2( ( _lastCommittedBlockTimeStamp >= MODERN_TIME || _lastCommittedBlockID == 0 ),
            "Invalid last committed block time stamp " );


        lastCommittedBlockTimeStamp = _lastCommittedBlockTimeStamp;

    } catch ( exception& e ) {
        SkaleException::logNested( e );
        throw_with_nested( EngineInitException( "Engine construction failed", __CLASS_NAME__ ) );
    }
};


ConsensusExtFace* ConsensusEngine::getExtFace() const {
    return extFace;
}


void ConsensusEngine::exitGracefullyBlocking() {
    exitGracefully();

    while ( getStatus() != CONSENSUS_EXITED ) {
        usleep( 100000 );
    }
}


void ConsensusEngine::exitGracefully() {
    // run and forget
    thread( [this]() { exitGracefullyAsync(); } ).detach();
}

consensus_engine_status ConsensusEngine::getStatus() const {
    return status;
}

void ConsensusEngine::exitGracefullyAsync() {
    try {
        auto previouslyCalled = exitRequested.exchange( true );

        if ( previouslyCalled ) {
            return;
        }


        for ( auto&& it : nodes ) {
            try {
                it.second->exit();
            } catch ( exception& e ) {
                SkaleException::logNested( e );
            }
        }


        threadRegistry->joinAll();

        for ( auto&& it : nodes ) {
            it.second->getSchain()->joinMonitorThread();
        }

    } catch ( exception& e ) {
        SkaleException::logNested( e );
        status = CONSENSUS_EXITED;
    }
    status = CONSENSUS_EXITED;
}


ConsensusEngine::~ConsensusEngine() {
    exitGracefullyBlocking();
    for ( auto& n : nodes ) {
        assert( n.second->isExitRequested() );
    }
    nodes.clear();
}


block_id ConsensusEngine::getLargestCommittedBlockID() {
    block_id id = 0;

    for ( auto&& item : nodes ) {
        auto id2 = item.second->getSchain()->getLastCommittedBlockID();

        if ( id2 > id ) {
            id = id2;
        }
    }

    return id;
}

u256 ConsensusEngine::getPriceForBlockId( uint64_t _blockId ) const {
    ASSERT( nodes.size() == 1 );

    for ( auto&& item : nodes ) {
        return item.second->getSchain()->getPriceForBlockId( _blockId );
    }

    return 0;  // never happens
}


bool ConsensusEngine::onTravis = false;

bool ConsensusEngine::noUlimitCheck = false;

bool ConsensusEngine::isOnTravis() {
    return onTravis;
}

bool ConsensusEngine::isNoUlimitCheck() {
    return noUlimitCheck;
}


set< node_id >& ConsensusEngine::getNodeIDs() {
    return nodeIDs;
}


string ConsensusEngine::engineVersion = ENGINE_VERSION;

string ConsensusEngine::getEngineVersion() {
    return engineVersion;
}

uint64_t ConsensusEngine::getEngineID() const {
    return engineID;
}

ptr< GlobalThreadRegistry > ConsensusEngine::getThreadRegistry() const {
    return threadRegistry;
}

shared_ptr< string > ConsensusEngine::getHealthCheckDir() const {
    return healthCheckDir;
}

ptr< string > ConsensusEngine::getDbDir() const {
    return dbDir;
}
void ConsensusEngine::setTestKeys( ptr< string > _sgxServerUrl, string _configFile,
    uint64_t _totalNodes, uint64_t _requiredNodes ) {
    CHECK_ARGUMENT( _sgxServerUrl );

    sgxServerUrl = _sgxServerUrl;

    CHECK_STATE( !useTestSGXKeys )
    CHECK_STATE( !isSGXEnabled )

    tie( ecdsaKeyNames, ecdsaPublicKeys, blsKeyNames, blsPublicKeys, blsPublicKey ) =
        JSONFactory::parseTestKeyNamesFromJson(
            sgxServerUrl, _configFile, _totalNodes, _requiredNodes );

    CHECK_STATE( ecdsaKeyNames );
    CHECK_STATE( ecdsaPublicKeys );
    CHECK_STATE( blsKeyNames );
    CHECK_STATE( blsPublicKeys );
    CHECK_STATE( blsPublicKey );

    isSGXEnabled = true;
    useTestSGXKeys = true;
}
void ConsensusEngine::setSGXKeyInfo( ptr< string > _sgxServerURL,
    ptr< string > _sgxSSLKeyFileFullPath, ptr< string > _sgxSSLCertFileFullPath,
    ptr< string > _ecdsaKeyName, ptr< vector< string > > _ecdsaPublicKeys,
    ptr< string > _blsKeyName, ptr< vector< ptr< vector< string > > > > _blsPublicKeyShares,
    uint64_t _requiredSigners, uint64_t _totalSigners ) {
    CHECK_STATE( _sgxServerURL );
    CHECK_STATE( _ecdsaKeyName );
    CHECK_STATE( _blsKeyName );
    CHECK_STATE( _blsPublicKeyShares );
    CHECK_STATE( _ecdsaPublicKeys );
    CHECK_STATE( _ecdsaPublicKeys );
    CHECK_STATE(_totalSigners >= _requiredSigners);

    this->sgxServerUrl = _sgxServerURL;
    this->isSGXEnabled = true;
    this->useTestSGXKeys = false;

    this->blsPublicKeys = _blsPublicKeyShares;
    this->ecdsaPublicKeys = _ecdsaPublicKeys;
    setEcdsaKeyName( _ecdsaKeyName );
    setBlsKeyName( _blsKeyName );


    map< size_t, shared_ptr< BLSPublicKeyShare > > blsPubKeyShares;


    for ( uint64_t i = 0; i < _requiredSigners; i++ ) {
        BLSPublicKeyShare pubKey( blsPublicKeys->at( i ), _requiredSigners, _totalSigners );

        blsPubKeyShares[i + 1] = make_shared< BLSPublicKeyShare >( pubKey );
    }

    // create pub key

    BLSPublicKey blsPublicKeyObj(
        make_shared< map< size_t, shared_ptr< BLSPublicKeyShare > > >( blsPubKeyShares ),
        _requiredSigners, _totalSigners );

    blsPublicKey = blsPublicKeyObj.toString();

    sgxSSLCertFileFullPath = _sgxSSLCertFileFullPath;
    sgxSSLKeyFileFullPath = _sgxSSLKeyFileFullPath;
}
ptr< string > ConsensusEngine::getEcdsaKeyName() const {
    CHECK_STATE(ecdsaKeyName);
    return ecdsaKeyName;
}
ptr< string > ConsensusEngine::getBlsKeyName() const {
    CHECK_STATE(blsKeyName);
    return blsKeyName;
}
void ConsensusEngine::setEcdsaKeyName( ptr< string > _ecdsaKeyName ) {
    CHECK_ARGUMENT(_ecdsaKeyName);
    CHECK_STATE( JSONFactory::splitString( *_ecdsaKeyName )->size() == 2 );
    ecdsaKeyName = _ecdsaKeyName;
}
void ConsensusEngine::setBlsKeyName( ptr< string > _blsKeyName ) {
    CHECK_ARGUMENT(_blsKeyName);
    CHECK_STATE( JSONFactory::splitString( *_blsKeyName )->size() == 7 );
    blsKeyName = _blsKeyName;
}

void ConsensusEngine::setTotalStorageLimitBytes( uint64_t _storageLimitBytes ) {
    totalStorageLimitBytes =  _storageLimitBytes;
}
uint64_t ConsensusEngine::getTotalStorageLimitBytes() const {
    return totalStorageLimitBytes;
}
ptr< StorageLimits > ConsensusEngine::getStorageLimits() const {
    return storageLimits;
}
