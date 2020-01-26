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





extern thread_local ptr<Log> logThreadLocal_;

using namespace spdlog::level;


class GlobalThreadRegistry;


class ConsensusEngine : public ConsensusInterface {

    static string engineVersion;

    ptr<GlobalThreadRegistry> threadRegistry;

    uint64_t engineID;

    static atomic<uint64_t> engineCounter;

    static shared_ptr< spdlog::logger > configLogger;

    static shared_ptr< string > dataDir;

    shared_ptr<string> healthCheckDir;
    shared_ptr<string> dbDir;
public:
    ptr<string> getDbDir() const;


private:

    static recursive_mutex logMutex;


    shared_ptr< string > logFileNamePrefix;

    shared_ptr< spdlog::sinks::sink > logRotatingFileSync;



public:



    void logInit();

    static void setConfigLogLevel( string& _s );

    ptr<string> getHealthCheckDir() const;



    static void log( level_enum _severity, const string& _message, const string& _className );

    static void logConfig(level_enum _severity, const string &_message, const string &_className);

    shared_ptr< spdlog::logger > createLogger( const string& loggerName );

    static const shared_ptr< string > getDataDir();


    recursive_mutex mutex;

    std::atomic<bool> exitRequested;

    map<node_id, ptr<Node>> nodes;

    static bool onTravis;

    static bool noUlimitCheck;

    std::string exec(const char *cmd);

    static void checkExistsAndDirectory(const fs_path &dirname);

    static void checkExistsAndFile(const fs_path &filename);

    ptr<Node> readNodeConfigFileAndCreateNode(const fs_path &path, set<node_id> &nodeIDs, bool _useSGX = false,
                                              ptr<string> _keyName = nullptr, ptr<vector<string>> _publicKeys = nullptr);


    void readSchainConfigFiles(ptr<Node> _node, const fs_path &_dirPath);

    ConsensusExtFace *extFace = nullptr;

    block_id lastCommittedBlockID = 0;

    uint64_t lastCommittedBlockTimeStamp = 0;

    string blsPublicKey1;
    string blsPublicKey2;
    string blsPublicKey3;
    string blsPublicKey4;
    string blsPrivateKey;

    set<node_id> nodeIDs;



    set<node_id> &getNodeIDs();

    static bool isOnTravis();

    static bool isNoUlimitCheck();

    node_count nodesCount();

    block_id getLargestCommittedBlockID();

    ConsensusEngine();

    ~ConsensusEngine() override;

    ConsensusEngine(ConsensusExtFace &_extFace, uint64_t _lastCommittedBlockID,
                    uint64_t _lastCommittedBlockTimeStamp, const string &_blsSecretKey = "",
                    const string &_blsPublicKey1 = "", const string &_blsPublicKey2 = "",
                    const string &_blsPublicKey3 = "", const string &_blsPublicKey4 = "");

    ConsensusExtFace *getExtFace() const;


    uint64_t getEngineID() const;



    void startAll() override;

    void parseFullConfigAndCreateNode(const string &fullPathToConfigFile) override;

    // used for standalone debugging

    void parseTestConfigsAndCreateAllNodes(const fs_path &dirname,
            bool useSGX = false, ptr<vector<string>> keyNames = nullptr, ptr<vector<string>>
            publicKeys = nullptr);

    void exitGracefully() override;

    void bootStrapAll() override;

    uint64_t getEmptyBlockIntervalMs() const {
        // HACK assume there is exactly one
        return (*(this->nodes.begin())).second->getEmptyBlockIntervalMs();
    }

    void setEmptyBlockIntervalMs(uint64_t _interval) {
        // HACK assume there is exactly one
        (*(this->nodes.begin())).second->setEmptyBlockIntervalMs(_interval);
    }

    // tests

    void slowStartBootStrapTest();

    void init();

    void joinAllThreads() const;

    const string &getBlsPublicKey1() const;

    const string &getBlsPublicKey2() const;

    const string &getBlsPublicKey3() const;

    const string &getBlsPublicKey4() const;

    const string &getBlsPrivateKey() const;

    u256 getPriceForBlockId(uint64_t _blockId) const override;

    void systemHealthCheck();


    static string getEngineVersion();

    ptr<GlobalThreadRegistry> getThreadRegistry() const;

};
