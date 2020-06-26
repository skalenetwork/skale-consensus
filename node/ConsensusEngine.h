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

    @file ConsensusEngine.h
    @author Stan Kladko
    @date 2018
*/

#pragma once


#include "SkaleCommon.h"
#include "boost/filesystem.hpp"
#include "Agent.h"
#include "thirdparty/json.hpp"
#include "threads/WorkerThreadPool.h"
#include "ConsensusInterface.h"
#include "Node.h"



#include "spdlog/spdlog.h"



#include <boost/multiprecision/cpp_int.hpp>

extern thread_local ptr< SkaleLog > logThreadLocal_;

using namespace spdlog::level;


class GlobalThreadRegistry;
class StorageLimits;


class ConsensusEngine : public ConsensusInterface {
    bool useTestSGXKeys = false;
    bool isSGXEnabled = false;
    ptr< string > sgxServerUrl = nullptr;

    ptr<string> sgxSSLKeyFileFullPath = nullptr;
    ptr<string> sgxSSLCertFileFullPath = nullptr;

    ptr<string> ecdsaKeyName = nullptr;

    ptr<string> blsKeyName = nullptr;

    ptr< vector< string > > ecdsaKeyNames = nullptr;
    ptr< vector< string > > blsKeyNames = nullptr;
    ptr< vector< string > > ecdsaPublicKeys = nullptr;
    ptr< vector< ptr< vector< string > > > > blsPublicKeys = nullptr;
    ptr< vector< string > > blsPublicKey = nullptr;


    atomic< consensus_engine_status > status = CONSENSUS_ACTIVE;

    static string engineVersion;

    ptr< GlobalThreadRegistry > threadRegistry;

    uint64_t engineID;

    static atomic< uint64_t > engineCounter;

    static shared_ptr< spdlog::logger > configLogger;

    static shared_ptr< string > dataDir;

    shared_ptr< string > healthCheckDir;
    shared_ptr< string > dbDir;

    static recursive_mutex logMutex;


    shared_ptr< string > logFileNamePrefix;

    shared_ptr< spdlog::sinks::sink > logRotatingFileSync;

    uint64_t totalStorageLimitBytes = 0;

    ptr<StorageLimits> storageLimits = nullptr;

public:
    ptr< StorageLimits > getStorageLimits() const;

public:

    void setEcdsaKeyName( ptr< string > _ecdsaKeyName );
    void setBlsKeyName( ptr< string > _blsKeyName );

    ptr< string > getEcdsaKeyName() const;
    ptr< string > getBlsKeyName() const;


    ptr< string > getDbDir() const;

    void logInit();

    static void setConfigLogLevel( string& _s );

    ptr< string > getHealthCheckDir() const;

    static void log( level_enum _severity, const string& _message, const string& _className );

    static void logConfig( level_enum _severity, const string& _message, const string& _className );

    shared_ptr< spdlog::logger > createLogger( const string& loggerName );

    static const shared_ptr< string > getDataDir();

    recursive_mutex mutex;

    std::atomic< bool > exitRequested;

    map< node_id, ptr< Node > > nodes;

    static bool onTravis;

    static bool noUlimitCheck;

    std::string exec( const char* cmd );

    static void checkExistsAndDirectory( const fs_path& dirname );

    static void checkExistsAndFile( const fs_path& filename );

    ptr< Node > readNodeConfigFileAndCreateNode( const fs_path& path, set< node_id >& nodeIDs,
        bool _useSGX = false, ptr< string > _sgxSSLKeyFileFullPath = nullptr,
        ptr< string > _sgxSSLCertFileFullPath = nullptr, ptr< string > _ecdsaKeyName = nullptr,
        ptr< vector< string > > _ecdsaPublicKeys = nullptr, ptr< string > _blsKeyName = nullptr,
        ptr< vector< ptr< vector< string > > > > _blsPublicKeys = nullptr,
        ptr< vector< string > > _blsPublicKey = nullptr );


    void readSchainConfigFiles( ptr< Node > _node, const fs_path& _dirPath );

    ConsensusExtFace* extFace = nullptr;

    block_id lastCommittedBlockID = 0;

    uint64_t lastCommittedBlockTimeStamp = 0;

    set< node_id > nodeIDs;


    set< node_id >& getNodeIDs();

    static bool isOnTravis();

    static bool isNoUlimitCheck();

    node_count nodesCount();

    block_id getLargestCommittedBlockID();

    ConsensusEngine( block_id _lastId = 0 );

    ~ConsensusEngine() override;

    ConsensusEngine( ConsensusExtFace& _extFace, uint64_t _lastCommittedBlockID,
        uint64_t _lastCommittedBlockTimeStamp);

    ConsensusExtFace* getExtFace() const;


    uint64_t getEngineID() const;


    void startAll() override;

    void parseFullConfigAndCreateNode( const string& fullPathToConfigFile ) override;

    // used for standalone debugging

    void parseTestConfigsAndCreateAllNodes( const fs_path& dirname );

    void exitGracefullyBlocking();

    void exitGracefullyAsync();

    virtual void exitGracefully() override;

    /* consensus status for now can be CONSENSUS_ACTIVE and CONSENSUS_EXITED */
    virtual consensus_engine_status getStatus() const override;

    void bootStrapAll() override;

    uint64_t getEmptyBlockIntervalMs() const override {
        return ( *( this->nodes.begin() ) ).second->getEmptyBlockIntervalMs();
    }

    void setEmptyBlockIntervalMs( uint64_t _interval ) override {
        ( *( this->nodes.begin() ) ).second->setEmptyBlockIntervalMs( _interval );
    }

    // tests

    void slowStartBootStrapTest();

    void init();


    u256 getPriceForBlockId( uint64_t _blockId ) const override;

    void systemHealthCheck();


    static string getEngineVersion();

    ptr< GlobalThreadRegistry > getThreadRegistry() const;

    void setTestKeys( ptr< string > _sgxServerURL, string _configFile, uint64_t _totalNodes,
        uint64_t _requiredNodes );

    void setSGXKeyInfo(ptr< string > _sgxServerURL,
        ptr<string> _sgxSSLKeyFileFullPath,
        ptr<string> _sgxSSLCertFileFullPath,
        ptr<string> _ecdsaKeyName,
        // array of ECDSA publicKeys of all nodes, including this node
                       ptr<vector<string>> _ecdsaPublicKeys,
                       ptr<string> _blsKeyName,
        // array of BLS public key shares of all nodes, including this node
        // each BLS public key share is a vector of 4 strings.
                       ptr<vector<ptr<vector<string>>>> _blsPublicKeyShares,
                       uint64_t _requiredSigners,
                       uint64_t _totalSigners);

    void setTotalStorageLimitBytes(uint64_t _storageLimitBytes);
    uint64_t getTotalStorageLimitBytes() const;
};
