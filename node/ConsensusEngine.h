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

#include "datastructures/TimeStamp.h"

#include "spdlog/spdlog.h"

#include <boost/multiprecision/cpp_int.hpp>

extern thread_local ptr< SkaleLog > logThreadLocal_;

using namespace spdlog::level;


class GlobalThreadRegistry;
class StorageLimits;


class ConsensusEngine : public ConsensusInterface {
    bool useTestSGXKeys = false;
    bool isSGXEnabled = false;
    string sgxServerUrl = "";

    string sgxSSLKeyFileFullPath = "";
    string sgxSSLCertFileFullPath = "";

    string ecdsaKeyName = "";

    string blsKeyName = "";

    ptr< vector<string> > ecdsaKeyNames = nullptr;
    ptr< vector<string> > blsKeyNames = nullptr;
    ptr< vector<string> > ecdsaPublicKeys = nullptr;
    ptr< vector< ptr< vector<string>>>> blsPublicKeys = nullptr;
    ptr< BLSPublicKey > blsPublicKey = nullptr;


    atomic< consensus_engine_status > status = CONSENSUS_ACTIVE;

    ptr< GlobalThreadRegistry > threadRegistry;

    uint64_t engineID;

    static atomic< uint64_t > engineCounter;

    static shared_ptr< spdlog::logger > configLogger;

    static string dataDir;
    static string logDir;

    string healthCheckDir;
    string dbDir;

    static recursive_mutex logMutex;


    string logFileNamePrefix;

    shared_ptr< spdlog::sinks::sink > logRotatingFileSync;

    ptr<StorageLimits> storageLimits = nullptr;

public:

    // used for testing only
    ptr< map< uint64_t, ptr< NodeInfo > > > testNodeInfosByIndex = nullptr;
    ptr< map< uint64_t, ptr< NodeInfo > > > testNodeInfosById = nullptr;

    ptr< StorageLimits > getStorageLimits() const;

    void setEcdsaKeyName(const string& _ecdsaKeyName );
    void setBlsKeyName(const string& _blsKeyName );

    const string getEcdsaKeyName() const;
    const string getBlsKeyName() const;


    string getDbDir() const;

    void logInit();

    static void setConfigLogLevel( string& _s );

    const string& getHealthCheckDir() const;

    static void log( level_enum _severity, const string& _message, const string& _className );

    static void logConfig( level_enum _severity, const string& _message, const string& _className );

    shared_ptr< spdlog::logger > createLogger( const string& loggerName );

    static const string getDataDir();
    static const string getLogDir();

    recursive_mutex mutex;

    std::atomic< bool > exitRequested;

    map< node_id, ptr< Node > > nodes;

    static bool onTravis;

    static bool noUlimitCheck;

    std::string exec( const char* cmd );

    static void checkExistsAndDirectory( const fs_path& dirname );

    static void checkExistsAndFile( const fs_path& filename );

    ptr< Node > readNodeConfigFileAndCreateNode( const string path, set< node_id >& nodeIDs,
        bool _useSGX = false, string _sgxSSLKeyFileFullPath = "",
        string _sgxSSLCertFileFullPath = "", string _ecdsaKeyName = "",
        ptr< vector<string> > _ecdsaPublicKeys = nullptr, string _blsKeyName = "",
        ptr< vector< ptr< vector<string>>>> _blsPublicKeys = nullptr,
        ptr< BLSPublicKey > _blsPublicKey = nullptr );


    void readSchainConfigFiles(const ptr< Node >& _node, const fs_path& _dirPath );

    ConsensusExtFace* extFace = nullptr;

    block_id lastCommittedBlockID = 0;

    ptr<TimeStamp> lastCommittedBlockTimeStamp;

    set< node_id > nodeIDs;

    /* Returns an old block from the consensus storage.
     * The block is an EXACT COPY of the info that was earlier provided by
     * ConsensusExtface::createBlock(...)
     *
     * The return values are:
     *
     *  shared pointer to transaction vector
     *  timeStampSec
     *  timeStampMS
     *  gasPrice
     *  stateRoot
     *
     *  If the block is not found (block is too old or in the future), will return nullptr as transaction vector,
     *  the remaining return values will be set to zero
     *
     *   Example of usage:
     *
     *   auto [transactions, timestampS, timeStampMs, price, stateRoot]  = engine->getBlock(1);
     *   if (transactions != nullptr) {
     *      ...
     *   }
     */

    tuple<ptr<ConsensusExtFace::transactions_vector>,  uint32_t , uint32_t , u256, u256> getBlock(block_id _blockID);

    set< node_id >& getNodeIDs();

    static bool isOnTravis();

    [[maybe_unused]] static bool isNoUlimitCheck();

    node_count nodesCount();

    block_id getLargestCommittedBlockID();

    ConsensusEngine( block_id _lastId = 0 );

    ~ConsensusEngine() override;

    ConsensusEngine( ConsensusExtFace& _extFace, uint64_t _lastCommittedBlockID,
        uint64_t _lastCommittedBlockTimeStamp,uint64_t _lastCommittedBlockTimeStampMs );

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

    void setTestKeys(const string& _sgxServerURL, string _configFile, uint64_t _totalNodes,
        uint64_t _requiredNodes );

    void setSGXKeyInfo(const string& _sgxServerURL,
        string& _sgxSSLKeyFileFullPath,
        string& _sgxSSLCertFileFullPath,
        string& _ecdsaKeyName,
        // array of ECDSA publicKeys of all nodes, including this node
                       ptr<vector<string>>& _ecdsaPublicKeys,
                       string& _blsKeyName,
        // array of BLS public key shares of all nodes, including this node
        // each BLS public key share is a vector of 4 strings.
                       ptr<vector<ptr<vector<string>>>>& _blsPublicKeyShares,
                       uint64_t _requiredSigners,
                       uint64_t _totalSigners);

    void setTotalStorageLimitBytes(uint64_t _storageLimitBytes);

    uint64_t getTotalStorageLimitBytes() const;
};
