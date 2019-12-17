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


#include "../SkaleCommon.h"
#include "boost/filesystem.hpp"
#include "../Agent.h"
#include "../thirdparty/json.hpp"
#include "../threads/WorkerThreadPool.h"
#include "ConsensusInterface.h"
#include "Node.h"

class ConsensusBLSPublicKey;

class ConsensusBLSPrivateKeyShare;

#include <boost/multiprecision/cpp_int.hpp>

#include "ConsensusEngine.h"



class ConsensusEngine : public ConsensusInterface {


    static string engineVersion;

    recursive_mutex mutex;

    std::atomic<bool> exitRequested;

    map<node_id, ptr<Node>> nodes;

    static bool onTravis;
public:
    static bool isOnTravis();

    static bool isNoUlimitCheck();

private:

    static bool noUlimitCheck;

    std::string exec(const char *cmd);

    static void checkExistsAndDirectory(const fs_path &dirname);

    static void checkExistsAndFile(const fs_path &filename);

    ptr<Node> readNodeConfigFileAndCreateNode(const fs_path &path, set<node_id> &nodeIDs);


    void readSchainConfigFiles(ptr<Node> _node, const fs_path &_dirPath);

    ConsensusExtFace *extFace = nullptr;

    block_id lastCommittedBlockID = 0;

    uint64_t lastCommittedBlockTimeStamp = 0;

    string blsPublicKey1;
    string blsPublicKey2;
    string blsPublicKey3;
    string blsPublicKey4;
    string blsPrivateKey;


public:

    node_count nodesCount();

    block_id getLargestCommittedBlockID();

    ConsensusEngine();

    ~ConsensusEngine() override;

    ConsensusEngine(ConsensusExtFace &_extFace, uint64_t _lastCommittedBlockID,
                    uint64_t _lastCommittedBlockTimeStamp, const string &_blsSecretKey = "",
                    const string &_blsPublicKey1 = "", const string &_blsPublicKey2 = "",
                    const string &_blsPublicKey3 = "", const string &_blsPublicKey4 = "");

    ConsensusExtFace *getExtFace() const;


    void startAll() override;

    void parseFullConfigAndCreateNode(const string &fullPathToConfigFile) override;

    // used for standalone debugging

    void parseConfigsAndCreateAllNodes(const fs_path &dirname);

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
};
