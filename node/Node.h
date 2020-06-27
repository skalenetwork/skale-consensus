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

    @file Node.h
    @author Stan Kladko
    @date 2018
*/

#pragma once

#include <mutex>

using namespace std;

class Sockets;

class Network;

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

class ConsensusBLSSigShare;

class SHAHash;

class BLSPublicKey;

class BLSPrivateKeyShare;

class CacheLevelDB;
class BlockDB;
class BlockProposalDB;
class RandomDB;
class PriceDB;

class ProposalHashDB;
class ProposalVectorDB;
class MsgDB;
class ConsensusStateDB;

class TestConfig;

class BlockSigShareDB;

class DASigShareDB;
class DAProofDB;

namespace leveldb {
class DB;
}


enum PricingStrategyEnum { ZERO, DOS_PROTECT };


class Node {
    ConsensusEngine* consensusEngine;

    vector< Agent* > agents;

    node_id nodeID;

    std::mutex threadServerCondMutex;


    std::condition_variable threadServerConditionVariable;


    std::mutex threadClientCondMutex;

    std::condition_variable threadClientConditionVariable;


    std::atomic_bool startedServers;

    std::atomic_bool startedClients;

    std::atomic_bool exitRequested;

    ptr< SkaleLog > log = nullptr;
    ptr< string > name = nullptr;

    ptr< string > bindIP = nullptr;


    nlohmann::json cfg;

    network_port basePort = 0;

    uint64_t simulateNetworkWriteDelayMs = 0;

    PricingStrategyEnum DOS_PROTECT;

    ptr< Sockets > sockets = nullptr;

    ptr< Network > network = nullptr;

    ptr< Schain > sChain = nullptr;

    ptr< TestConfig > testConfig = nullptr;

    class Comparator {
    public:
        bool operator()(  ptr< string >& a,  ptr< string >& b )  { return *a < *b; }
    };


    ptr< map< uint64_t, ptr< NodeInfo > > > nodeInfosByIndex;
    ptr< map< uint64_t, ptr< NodeInfo > > > nodeInfosById;


    bool sgxEnabled = false;



    ptr< string > ecdsaKeyName = nullptr;
    ptr< vector< string > > ecdsaPublicKeys = nullptr;
    ptr< string > blsKeyName = nullptr;
    ptr< vector< ptr< vector< string > > > > blsPublicKeys = nullptr;
    ptr< vector< string > > blsPublicKey = nullptr;

    ptr<string> sgxURL = nullptr;



    ptr<string> sgxSSLKeyFileFullPath = nullptr;
    ptr<string> sgxSSLCertFileFullPath = nullptr;



    void releaseGlobalServerBarrier();

    void releaseGlobalClientBarrier();


    void closeAllSocketsAndNotifyAllAgentsAndThreads();


    ptr< BlockDB > blockDB = nullptr;

    ptr< RandomDB > randomDB = nullptr;

    ptr< PriceDB > priceDB = nullptr;

    ptr< ProposalHashDB > proposalHashDB = nullptr;

    ptr< ProposalVectorDB > proposalVectorDB = nullptr;

    ptr< MsgDB > outgoingMsgDB = nullptr;

    ptr< MsgDB > incomingMsgDB = nullptr;

    ptr< ConsensusStateDB > consensusStateDB = nullptr;


    ptr< BlockSigShareDB > blockSigShareDB = nullptr;

    ptr< DASigShareDB > daSigShareDB = nullptr;

    ptr< DAProofDB > daProofDB = nullptr;

    ptr< BlockProposalDB > blockProposalDB = nullptr;

    uint64_t catchupIntervalMS;

    uint64_t monitoringIntervalMS;

    uint64_t waitAfterNetworkErrorMs;

    uint64_t emptyBlockIntervalMs;

    uint64_t blockProposalHistorySize;

    uint64_t committedTransactionsHistory;

    uint64_t maxCatchupDownloadBytes;


    uint64_t maxTransactionsPerBlock;

    uint64_t minBlockIntervalMs;

    uint64_t blockDBSize;
    uint64_t proposalHashDBSize;
    uint64_t proposalVectorDBSize;
    uint64_t outgoingMsgDBSize;
    uint64_t incomingMsgDBSize;
    uint64_t consensusStateDBSize;
    uint64_t daSigShareDBSize;
    uint64_t daProofDBSize;
    uint64_t blockSigShareDBSize;
    uint64_t randomDBSize;
    uint64_t priceDBSize;
    uint64_t blockProposalDBSize;


public:
    ptr< string > getEcdsaKeyName();
    ptr< vector< string > > getEcdsaPublicKeys();
    ptr< string > getBlsKeyName();
    ptr< vector< ptr< vector< string > > > > getBlsPublicKeys();
    ptr< vector< string > > getBlsPublicKey();


    bool isSgxEnabled() ;

    ptr< TestConfig > getTestConfig();

    ptr< BlockDB > getBlockDB();
    ptr< RandomDB > getRandomDB();
    ptr< PriceDB > getPriceDB() ;
    ptr< ProposalHashDB > getProposalHashDB();
    ptr< ProposalVectorDB > getProposalVectorDB();
    ptr< MsgDB > getOutgoingMsgDB();
    ptr< MsgDB > getIncomingMsgDB();
    ptr< ConsensusStateDB > getConsensusStateDB();
    ptr< BlockSigShareDB > getBlockSigShareDB() ;
    ptr< DASigShareDB > getDaSigShareDB() ;
    ptr< DAProofDB > getDaProofDB() ;
    ptr< BlockProposalDB > getBlockProposalDB() ;


    uint64_t getProposalHashDBSize() ;
    uint64_t getProposalVectorDBSize() ;
    uint64_t getOutgoingMsgDBSize() ;
    uint64_t getIncomingMsgDBSize() ;
    uint64_t getConsensusStateDBSize() ;
    uint64_t getBlockDBSize() ;
    uint64_t getBlockSigShareDBSize() ;
    uint64_t getRandomDBSize() ;
    uint64_t getPriceDBSize() ;
    uint64_t getDaSigShareDBSize() ;
    uint64_t getDaProofDBSize() ;
    uint64_t getBlockProposalDBSize() ;
    uint64_t getSimulateNetworkWriteDelayMs() ;
    ptr< BLSPublicKey > getBlsPublicKey() ;


    void initLevelDBs();
    bool isStarted() ;


    Node(  nlohmann::json& _cfg, ConsensusEngine* _consensusEngine, bool _useSGX,
        ptr<string> _sgxURL,
          ptr<string> _sgxSSLKeyFileFullPath,
    ptr<string> _sgxSSLCertFileFullPath,
        ptr< string > _ecdsaKeyName, ptr< vector< string > > _ecdsaPublicKeys,
        ptr< string > _blsKeyName, ptr< vector< ptr< vector< string > > > > _blsPublicKeys,
        ptr< vector< string > > _blsPublicKey );

    ~Node();


    void startServers();

    void exit();

    void exitOnFatalError(  string& message );

    void setSchain( ptr< Schain > _schain );

    static void initSchain( ptr< Node > _node, ptr< NodeInfo > _localNodeInfo,
         vector< ptr< NodeInfo > >& remoteNodeInfos, ConsensusExtFace* _extFace );

    void waitOnGlobalServerStartBarrier( Agent* agent );

    void waitOnGlobalClientStartBarrier();

    ptr< SkaleLog > getLog() ;


    nlohmann::json getCfg() ;

    ptr< map< uint64_t, ptr< NodeInfo > > > getNodeInfosByIndex() ;

    node_id getNodeID() ;


    Sockets* getSockets() ;


    Schain* getSchain() ;

    void registerAgent( Agent* _agent );

    bool isExitRequested();

    void exitCheck();


    ptr< NodeInfo > getNodeInfoByIndex( schain_index _index );


    ptr< NodeInfo > getNodeInfoById( node_id _id );

    ptr< Network > getNetwork() ;

    ptr< string > getBindIP() ;

    network_port getBasePort() ;

    void setBasePort(  network_port& _basePort );

    uint64_t getCommittedTransactionHistoryLimit() ;


    void startClients();

    uint64_t getCatchupIntervalMs();

    uint64_t getMonitoringIntervalMs();


    uint64_t getEmptyBlockIntervalMs() ;

    uint64_t getMaxCatchupDownloadBytes() ;

    uint64_t getMaxTransactionsPerBlock() ;

    uint64_t getMinBlockIntervalMs() ;

    uint64_t getWaitAfterNetworkErrorMs();

    uint64_t getParamUint64(  string& _paramName, uint64_t paramDefault );

    int64_t getParamInt64(  string& _paramName, uint64_t _paramDefault );

    ptr< string > getParamString(  string& _paramName, string& _paramDefault );

    void initParamsFromConfig();

    void initLogging();

    void setEmptyBlockIntervalMs( uint64_t _interval ) { this->emptyBlockIntervalMs = _interval; }

    void setNodeInfo( ptr< NodeInfo > _nodeInfo );

    ConsensusEngine* getConsensusEngine() ;

    ptr< string > getSgxUrl();
    ptr< string > getSgxSslKeyFileFullPath();
    ptr< string > getSgxSslCertFileFullPath();
};
