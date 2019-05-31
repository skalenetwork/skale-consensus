/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skale-consensus.  If not, see <http://www.gnu.org/licenses/>.

    @file Node.h
    @author Stan Kladko
    @date 2018
*/

#pragma  once

class Sockets;

class TransportNetwork;
class ZMQNetwork;
class BlockProposalServerAgent;
class CatchupServerAgent;
class NetworkMessageEnvelope;
class NodeInfo;
class Schain;
class NodeInfo;
class Agent;
class ConsensusExtFace;
class ConsensusEngine;
class BLSSigShare;
class SHAHash;
class BLSPublicKey;
class BLSPrivateKey;
class LevelDB;

namespace leveldb{
    class DB;
}

class Node {

    ConsensusEngine* consensusEngine;

    vector< Agent* > agents;

    node_id nodeID;

    mutex threadServerCondMutex;


    condition_variable threadServerConditionVariable;


    mutex threadClientCondMutex;

    condition_variable threadClientConditionVariable;


    bool startedServers;

    bool startedClients;


    volatile bool exitRequested;

    ptr<Log> log = nullptr;
    ptr<string> name = nullptr;

    ptr<string> bindIP = nullptr;


    nlohmann::json cfg;

    network_port basePort = 0;

    uint64_t simulateNetworkWriteDelayMs = 0;

private:


    ptr<Sockets> sockets = nullptr;

    ptr<TransportNetwork> network = nullptr;

    ptr<Schain> sChain = nullptr;

    ptr<BlockProposalServerAgent> blockProposalServerAgent = nullptr;

    ptr<CatchupServerAgent> catchupServerAgent = nullptr;


    class Comparator {
    public:
        bool operator()(const ptr<string> &a,
                        const ptr<string> &b ) const {
            return *a < *b;
        }
    };


    map<schain_index, ptr<NodeInfo>> nodeInfosByIndex;
    map<ptr<string>, ptr<NodeInfo>, Comparator> nodeInfosByIP;


    void releaseGlobalServerBarrier();

    void releaseGlobalClientBarrier();


    void closeAllSocketsAndNotifyAllAgentsAndThreads();


    ptr<LevelDB> blocksDB = nullptr;

    ptr<LevelDB> randomDB = nullptr;

    ptr<LevelDB> committedTransactionsDB = nullptr;


    ptr<LevelDB> signaturesDB = nullptr;


    uint64_t catchupIntervalMS;

    uint64_t waitAfterNetworkErrorMs;

    uint64_t emptyBlockIntervalMs;

    uint64_t blockProposalHistorySize;

    uint64_t committedTransactionsHistory;

    uint64_t maxCatchupDownloadBytes;


    uint64_t maxTransactionsPerBlock;

    uint64_t minBlockIntervalMs;

    uint64_t committedBlockStorageSize;


    bool isBLSEnabled = false;
public:
    bool isBlsEnabled() const;

private:


    ptr<BLSPublicKey> blsPublicKey;


    ptr<BLSPrivateKey> blsPrivateKey;

public:

    uint64_t getSimulateNetworkWriteDelayMs() const;

    const ptr<BLSPublicKey> &getBlsPublicKey() const;

    const ptr<BLSPrivateKey> &getBlsPrivateKey() const;


    ptr<LevelDB> getBlocksDB();

    ptr<LevelDB> getRandomDB();


    ptr<LevelDB> getCommittedTransactionsDB() const;


    ptr<LevelDB> getSignaturesDB() const;



    void initLevelDBs();

    void cleanLevelDBs();


    bool isStarted() const;

    static set<node_id> nodeIDs;


    Node(const nlohmann::json &_cfg, ConsensusEngine* _consensusEngine);

    ~Node();


    void start();

    void exit();

    void exitOnFatalError(const string& message);



    void initSchain(ptr<NodeInfo> _localNodeInfo, const vector<ptr<NodeInfo>> &remoteNodeInfos,
                    ConsensusExtFace *_extFace);

    void waitOnGlobalServerStartBarrier(Agent *agent);

    void waitOnGlobalClientStartBarrier(Agent *agent);

    const ptr<Log> &getLog() const;


    const nlohmann::json &getCfg() const;

    const map<schain_index, ptr<NodeInfo>> &getNodeInfosByIndex() const;

    node_id getNodeID() const;


    Sockets *getSockets() const;


    Schain *getSchain() const;

    vector<Agent *> &getAgents();

    bool isExitRequested();

    void exitCheck();


    ptr<NodeInfo> getNodeInfoByIndex(schain_index _index);


    ptr<NodeInfo> getNodeInfoByIP(ptr<string> ip);

    const ptr<TransportNetwork> &getNetwork() const;

    const ptr<string> &getBindIP() const;

    const network_port &getBasePort() const;

    void setBasePort(const network_port &_basePort);

    uint64_t getCommittedTransactionHistoryLimit() const;


    void startClients();

    uint64_t  getCatchupIntervalMs();

    ConsensusEngine *getConsensusEngine() const;

    uint64_t getEmptyBlockIntervalMs() const;

    uint64_t getBlockProposalHistorySize() const;

    uint64_t getMaxCatchupDownloadBytes() const;

    uint64_t getSocketBacklog() const;

    uint64_t getMaxTransactionsPerBlock() const;

    uint64_t getMinBlockIntervalMs() const;



    uint64_t getCommittedBlockStorageSize() const;


    uint64_t getWaitAfterNetworkErrorMs();


    uint64_t getParamUint64(const string &_paramName, uint64_t paramDefault);

    int64_t getParamInt64(const string &_paramName, uint64_t _paramDefault);

    void initParamsFromConfig();

    void initLogging();

};
