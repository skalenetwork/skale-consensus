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

    You should have received a copy of the GNU Affero General fPublic License
    along with skale-consensus.  If not, see <https://www.gnu.org/licenses/>.

    @file Node.cpp
    @author Stan Kladko
    @date 2018
*/

#include "leveldb/db.h"

#include "SkaleCommon.h"
#include "Log.h"


#include "exceptions/ExitRequestedException.h"
#include "exceptions/FatalError.h"
#include "exceptions/InvalidArgumentException.h"
#include "exceptions/ParsingException.h"
#include "thirdparty/json.hpp"

#include "protocols/blockconsensus/BlockConsensusAgent.h"

#include "chains/TestConfig.h"
#include "crypto/bls_include.h"
#include "crypto/CryptoManager.h"
#include "libBLS/bls/BLSPublicKey.h"

#include "ConsensusEngine.h"
#include "ConsensusInterface.h"

#include "blockproposal/server/BlockProposalServerAgent.h"
#include "catchup/server/CatchupServerAgent.h"
#include "chains/Schain.h"
#include "db/BlockDB.h"
#include "db/BlockProposalDB.h"
#include "db/BlockSigShareDB.h"
#include "db/ConsensusStateDB.h"
#include "db/DAProofDB.h"
#include "db/DASigShareDB.h"
#include "db/InternalInfoDB.h"
#include "db/MsgDB.h"
#include "db/PriceDB.h"
#include "db/ProposalHashDB.h"
#include "db/ProposalVectorDB.h"
#include "db/RandomDB.h"
#include "db/SigDB.h"
#include "messages/Message.h"
#include "messages/NetworkMessageEnvelope.h"
#include "network/Sockets.h"
#include "network/TCPServerSocket.h"
#include "network/ZMQNetwork.h"
#include "network/ZMQSockets.h"
#include "json/JSONFactory.h"
#include "db/StorageLimits.h"
#include "NodeInfo.h"
#include "Node.h"

#include <chrono>

using namespace std;


const string& Node::getGethUrl() const {
    return gethURL;
}


Node::Node(const nlohmann::json &_cfg, ConsensusEngine *_consensusEngine,
           bool _useSGX, string _sgxURL,
           string _sgxSSLKeyFileFullPath,
           string _sgxSSLCertFileFullPath,
           string _ecdsaKeyName,
           ptr< vector<string> > _ecdsaPublicKeys, string _blsKeyName,
           ptr< vector< ptr< vector<string>>>> _blsPublicKeys,
           ptr< BLSPublicKey > _blsPublicKey, string & _gethURL,
           ptr< map< uint64_t, ptr< BLSPublicKey > > > _previousBlsPublicKeys,
           ptr< map< uint64_t, string > > _historicECDSAPublicKeys,
           ptr< map< uint64_t, vector< uint64_t > > > _historicNodeGroups,
           bool _isSyncNode) : gethURL(_gethURL), isSyncNode(_isSyncNode) {

    historicECDSAPublicKeys = make_shared<map<uint64_t, string>>();
    historicNodeGroups = make_shared<map<uint64_t, vector< uint64_t>>>();



    // trim slash from URL
    if (!gethURL.empty() && gethURL.at(gethURL.size() - 1) == '/') {
        gethURL = gethURL.substr(0, gethURL.size() - 1);
    }



    ecdsaPublicKeys = _ecdsaPublicKeys;
    blsPublicKeys = _blsPublicKeys;
    blsPublicKey = _blsPublicKey;
    previousBlsPublicKeys = _previousBlsPublicKeys;
    historicECDSAPublicKeys = _historicECDSAPublicKeys;
    historicNodeGroups = _historicNodeGroups;

    sgxEnabled = _useSGX;

    if (sgxEnabled) {
        CHECK_ARGUMENT(!_sgxURL.empty())
        if (_sgxURL.find("https:/") != string::npos) {
            CHECK_ARGUMENT(!_sgxSSLKeyFileFullPath.empty() )
            CHECK_ARGUMENT(!_sgxSSLCertFileFullPath.empty() )
        }
        CHECK_ARGUMENT(!_ecdsaKeyName.empty() && _ecdsaPublicKeys);
        CHECK_ARGUMENT(!_blsKeyName.empty() && _blsPublicKeys);
        CHECK_ARGUMENT(_blsPublicKey);
        CHECK_ARGUMENT(_previousBlsPublicKeys);
        CHECK_ARGUMENT(_historicECDSAPublicKeys);
        CHECK_ARGUMENT(_historicNodeGroups);

        CHECK_STATE(JSONFactory::splitString(_ecdsaKeyName)->size() == 2);
        CHECK_STATE(JSONFactory::splitString(_blsKeyName)->size() == 7);

        ecdsaKeyName = _ecdsaKeyName;

        blsKeyName = _blsKeyName;


        static string empty("");

        sgxURL = _sgxURL;
        sgxSSLKeyFileFullPath = _sgxSSLKeyFileFullPath;
        sgxSSLCertFileFullPath = _sgxSSLCertFileFullPath;


    }

    this->consensusEngine = _consensusEngine;
    this->nodeInfosByIndex = make_shared<map<uint64_t , ptr<NodeInfo> > >();
    this->nodeInfosById = make_shared<map<uint64_t , ptr<NodeInfo>> >();

    this->startedServers = false;
    this->startedClients = false;
    this->exitRequested = false;
    this->cfg = _cfg;

    try {
        initParamsFromConfig();
    } catch (...) {
        throw_with_nested(ParsingException("Could not parse params", __CLASS_NAME__));
    }
    initLogging();
}

void Node::initLevelDBs() {
    auto dbDir = consensusEngine->getDbDir();
    string blockDBPrefix = "blocks_" + to_string(nodeID) + ".db";
    string randomDBPrefix = "randoms_" + to_string(nodeID) + ".db";
    string priceDBPrefix = "prices_" + to_string(nodeID) + ".db";
    string proposalHashDBPrefix = "/proposal_hashes_" + to_string(nodeID) + ".db";
    string proposalVectorDBPrefix = "/proposal_vectors_" + to_string(nodeID) + ".db";
    string outgoingMsgDBPrefix = "/outgoing_msgs_" + to_string(nodeID) + ".db";
    string incomingMsgDBPrefix = "/incoming_msgs_" + to_string(nodeID) + ".db";
    string consensusStateDBPrefix = "/consensus_state_" + to_string(nodeID) + ".db";
    string blockSigShareDBPrefix = "/block_sigshares_" + to_string(nodeID) + ".db";
    string daSigShareDBPrefix = "/da_sigshares_" + to_string(nodeID) + ".db";
    string daProofDBPrefix = "/da_proofs_" + to_string(nodeID) + ".db";
    string blockProposalDBPrefix = "/block_proposals_" + to_string(nodeID) + ".db";
    string internalInfoDBPrefix = "/internal_info_" + to_string(nodeID) + ".db";


    blockDB = make_shared<BlockDB>(getSchain(), dbDir, blockDBPrefix, getNodeID(), getBlockDBSize());
    randomDB = make_shared<RandomDB>(getSchain(), dbDir, randomDBPrefix, getNodeID(), getRandomDBSize());
    priceDB = make_shared<PriceDB>(getSchain(), dbDir, priceDBPrefix, getNodeID(), getPriceDBSize());
    proposalHashDB = make_shared<ProposalHashDB>(getSchain(), dbDir, proposalHashDBPrefix, getNodeID(),
                                                 getProposalHashDBSize());
    proposalVectorDB = make_shared<ProposalVectorDB>(getSchain(), dbDir, proposalVectorDBPrefix, getNodeID(),
                                                 getProposalVectorDBSize());

    outgoingMsgDB = make_shared<MsgDB>(getSchain(), dbDir, outgoingMsgDBPrefix, getNodeID(),
                                       getOutgoingMsgDBSize());

    incomingMsgDB = make_shared<MsgDB>(getSchain(), dbDir, incomingMsgDBPrefix, getNodeID(),
                                       getIncomingMsgDBSize());

    consensusStateDB = make_shared<ConsensusStateDB>(getSchain(), dbDir, consensusStateDBPrefix, getNodeID(),
                                       getConsensusStateDBSize());


    blockSigShareDB = make_shared<BlockSigShareDB>(getSchain(), dbDir, blockSigShareDBPrefix, getNodeID(),
                                                   getBlockSigShareDBSize());
    daSigShareDB = make_shared<DASigShareDB>(getSchain(), dbDir, daSigShareDBPrefix, getNodeID(),
                                             getDaSigShareDBSize());
    daProofDB = make_shared<DAProofDB>(getSchain(), dbDir, daProofDBPrefix, getNodeID(), getDaProofDBSize());
    blockProposalDB = make_shared<BlockProposalDB>(getSchain(), dbDir, blockProposalDBPrefix, getNodeID(),
                                                   getBlockProposalDBSize());

    internalInfoDB = make_shared<InternalInfoDB>(getSchain(), dbDir, internalInfoDBPrefix, getNodeID(),
                                                   getInternalInfoDBSize());


}

void Node::initLogging() {
    log = make_shared< SkaleLog >(nodeID, getConsensusEngine());

    if (cfg.find("logLevel") != cfg.end()) {
        string logLevel = cfg.at("logLevel").get<string>();
        log->setGlobalLogLevel(logLevel);
    }

    for (auto &&item : log->loggers) {
        string category = "logLevel" + item.first;
        if (cfg.find(category) != cfg.end()) {
            string logLevel = cfg.at(category).get<string>();
            LOG(info, "Setting log level:" + category + ":" + logLevel);
            log->loggers[item.first]->set_level( SkaleLog::logLevelFromString(logLevel));
        }
    }
}


void Node::initParamsFromConfig() {

    auto engine = getConsensusEngine();
    CHECK_STATE(engine);
    auto storageLimits = engine->getStorageLimits();
    CHECK_STATE(storageLimits);

    nodeID = cfg.at("nodeID").get<uint64_t>();
    name = cfg.at("nodeName").get<string>();
    bindIP = cfg.at("bindIP").get<string>();
    basePort = network_port(cfg.at("basePort").get<int>());

    catchupIntervalMS = getParamUint64("catchupIntervalMs", CATCHUP_INTERVAL_MS);
    monitoringIntervalMs = getParamUint64("monitoringIntervalMs", MONITORING_INTERVAL_MS);
    stuckMonitoringIntervalMs = getParamUint64("stuckMonitoringIntervalMs", STUCK_MONITORING_INTERVAL_MS);
    stuckRestartIntervalMs = getParamUint64("stuckRestartIntervalMs", STUCK_RESTART_INTERVAL_MS);
    waitAfterNetworkErrorMs = getParamUint64("waitAfterNetworkErrorMs", WAIT_AFTER_NETWORK_ERROR_MS);
    blockProposalHistorySize = getParamUint64("blockProposalHistorySize", BLOCK_PROPOSAL_HISTORY_SIZE);
    committedTransactionsHistory = getParamUint64("committedTransactionsHistory", COMMITTED_TRANSACTIONS_HISTORY);
    maxCatchupDownloadBytes = getParamUint64("maxCatchupDownloadBytes", MAX_CATCHUP_DOWNLOAD_BYTES);
    maxTransactionsPerBlock = getParamUint64("maxTransactionsPerBlock", MAX_TRANSACTIONS_PER_BLOCK);
    minBlockIntervalMs = getParamUint64("minBlockIntervalMs", MIN_BLOCK_INTERVAL_MS);


    blockDBSize = storageLimits->getBlockDbSize();
    proposalHashDBSize = storageLimits->getProposalHashDbSize();
    proposalVectorDBSize = storageLimits->getProposalVectorDbSize();
    outgoingMsgDBSize = storageLimits->getOutgoingMsgDbSize();
    incomingMsgDBSize = storageLimits->getIncomingMsgDbSize();
    consensusStateDBSize = storageLimits->getConsensusStateDbSize();
    blockSigShareDBSize = storageLimits->getBlockSigShareDbSize();
    daSigShareDBSize = storageLimits->getDaSigShareDbSize();
    daProofDBSize = storageLimits->getDaProofDbSize();
    randomDBSize = storageLimits->getRandomDbSize();
    priceDBSize = storageLimits->getPriceDbSize();
    blockProposalDBSize = storageLimits->getBlockProposalDbSize();
    internalInfoDBSize = storageLimits->getInternalInfoDbSize();

    visualizationType = getParamUint64("visualizationType", 0);

    simulateNetworkWriteDelayMs = getParamInt64("simulateNetworkWriteDelayMs", 0);

    testConfig = make_shared<TestConfig>(cfg);
}

uint64_t Node::getProposalHashDBSize() const {
    return proposalHashDBSize;
}

uint64_t Node::getProposalVectorDBSize() const {
    return proposalVectorDBSize;
}

uint64_t Node::getOutgoingMsgDBSize() const {
    return outgoingMsgDBSize;
}

uint64_t Node::getIncomingMsgDBSize() const {
    return incomingMsgDBSize;
}

Node::~Node() {}


void Node::startServers() {

    CHECK_STATE(!startedServers);

    // temporarily set last committed block id to last block saved in consensus
    // this is done to filter out old messages until last committed block id is set in
    // bootstrap all
    auto lastCommittedBlockIDInConsensus = getBlockDB()->readLastCommittedBlockID();
    sChain->setLastCommittedBlockId((uint64_t ) lastCommittedBlockIDInConsensus);

    LOG(info, "Starting node on");

    LOG(trace, "Initing sockets");

    this->sockets = make_shared<Sockets>(*this);

    sockets->initSockets(bindIP, (uint16_t) basePort);

    LOG(trace, "Constructing servers");

    sChain->constructServers(sockets);

    LOG(trace, " Creating consensus network");

    if (!isSyncOnlyNode()) {
       network = make_shared<ZMQNetwork>(*sChain);

       LOG(trace, " Starting consensus messaging");

       network->startThreads();
    }

    LOG(trace, "Starting schain");

    sChain->startThreads();

    LOG(trace, "Releasing server threads");

    releaseGlobalServerBarrier();
}



void Node::startClients() {
    if( isExitRequested() )
        return;
    sChain->healthCheck();
    if( isExitRequested() )
        return;
    releaseGlobalClientBarrier();
}

void Node::testNodeInfos() {
    auto engine = getConsensusEngine();
    CHECK_STATE(engine);
    if (engine->testNodeInfosById == nullptr) {
        engine->testNodeInfosById = nodeInfosById;
    } else {
        CHECK_STATE(engine->testNodeInfosById->size() == nodeInfosById->size())
        for (auto&& item : *engine->testNodeInfosById) {
            CHECK_STATE2(nodeInfosById->count(item.first) == 1,
                "Could not find node_id " + to_string(item.first) );
            CHECK_STATE(nodeInfosById->at(item.first)->getNodeID() == item.second->getNodeID());
            CHECK_STATE(nodeInfosById->at(item.first)->getSchainIndex() == item.second->getSchainIndex());
        }
    }

    if (engine->testNodeInfosByIndex == nullptr) {
        CHECK_STATE(nodeInfosByIndex);
        engine->testNodeInfosByIndex = nodeInfosByIndex;
    } else {
        CHECK_STATE(engine->testNodeInfosByIndex->size() == nodeInfosByIndex->size())
        for (auto&& item : *engine->testNodeInfosByIndex) {
            CHECK_STATE2(nodeInfosByIndex->count(item.first) == 1,
                         "Could not find schain index " + to_string(item.first) );
            CHECK_STATE(nodeInfosByIndex->at(item.first)->getNodeID() == item.second->getNodeID());
            CHECK_STATE(nodeInfosByIndex->at(item.first)->getSchainIndex() == item.second->getSchainIndex());
        }
    }

    ptr< map< uint64_t, ptr< NodeInfo > > > testNodeInfosByIndex;
    ptr< map< uint64_t, ptr< NodeInfo > > > testNodeInfosById;

}


void Node::setNodeInfo(const ptr<NodeInfo>& _nodeInfo) {

    CHECK_ARGUMENT(_nodeInfo);
    (*nodeInfosByIndex)[(uint64_t)_nodeInfo->getSchainIndex()] =  _nodeInfo;
    (*nodeInfosById)[(uint64_t ) _nodeInfo->getNodeID()] = _nodeInfo;
}

void Node::setSchain(const ptr<Schain>& _schain) {
    CHECK_STATE(sChain == nullptr);
    this->sChain = _schain;
    initLevelDBs();

    this->inited = true;
}

void Node::initSchain(const ptr<Node>& _node, schain_index _schainIndex, schain_id _schainId, const vector<ptr<NodeInfo> > &remoteNodeInfos,
                      ConsensusExtFace *_extFace, string& _schainName) {


    set<string> ipPortSet;

    try {
        logThreadLocal_ = _node->getLog();

        for (auto &rni : remoteNodeInfos) {
            LOG(debug, "Adding Node Info:" + to_string(rni->getSchainIndex()));
            _node->setNodeInfo(rni);
            LOG(debug, "Got IP" + rni->getBaseIP());

            auto ipPortString = rni->getBaseIP() + ":" + to_string((uint16_t ) rni->getPort());

            LOG(info, "Adding:" + ipPortString);

            if (ipPortSet.count(ipPortString) > 0 ) {
                BOOST_THROW_EXCEPTION(InvalidStateException("Double entry is found in schain config:  "
                    + ipPortString,
                        __CLASS_NAME__));
            } else {
                ipPortSet.insert(ipPortString);
            }
        }
        _node->testNodeInfos();

        auto chain = make_shared<Schain>(
                _node, _schainIndex, _schainId, _extFace, _schainName);

        _node->setSchain(chain);

        if (_node->isSyncOnlyNode()) {
            return;
        }

        chain->createBlockConsensusInstance();
        chain->createOracleInstance();

    } catch (...) {
        throw_with_nested(FatalError(__FUNCTION__, __CLASS_NAME__));
    }

}


void Node::waitOnGlobalServerStartBarrier(Agent * _agent ) {
    CHECK_ARGUMENT( _agent );
    logThreadLocal_ = _agent->getSchain()->getNode()->getLog();
    std::unique_lock<std::mutex> mlock(threadServerCondMutex);
    while (!startedServers) {
        if( exitRequested )
            break;
        // threadServerConditionVariable.wait(mlock);
        threadServerConditionVariable.wait_for( mlock, std::chrono::milliseconds( 1000 ) );
    }
}

void Node::releaseGlobalServerBarrier() {

    RETURN_IF_PREVIOUSLY_CALLED(startedServers)

    std::lock_guard<std::mutex> lock(threadServerCondMutex);

    threadServerConditionVariable.notify_all();
}


void Node::waitOnGlobalClientStartBarrier() {
    logThreadLocal_ = getLog();
    std::unique_lock<std::mutex> mlock(threadClientCondMutex);
    while (!startedClients) {
        if( exitRequested )
            break;
        // threadClientConditionVariable.wait(mlock);
        threadClientConditionVariable.wait_for( mlock, std::chrono::milliseconds( 1000 ) );
    }
}


void Node::releaseGlobalClientBarrier() {

    RETURN_IF_PREVIOUSLY_CALLED(startedClients)

    std::lock_guard<std::mutex> lock(threadClientCondMutex);

    threadClientConditionVariable.notify_all();
}

void Node::exit() {

    LOG(info, "Node::exit() requested");

    getSchain()->stopStatusServer();

    RETURN_IF_PREVIOUSLY_CALLED(exitRequested);

    releaseGlobalClientBarrier();
    releaseGlobalServerBarrier();


    closeAllSocketsAndNotifyAllAgentsAndThreads();

}


void Node::closeAllSocketsAndNotifyAllAgentsAndThreads() {

    LOG(info, "consensus engine exiting: close all sockets called");

    getSchain()->getNode()->threadServerConditionVariable.notify_all();

    LOG(info, "consensus engine exiting: server conditional vars notified");

    {
        LOCK( agentsLock );

        CHECK_STATE( agents.size() > 0 );

        for ( auto&& agent : agents ) {
            agent->notifyAllConditionVariables();
        }
    }

    LOG(info, "consensus engine exiting: agent conditional vars notified");

    if (sockets && sockets->catchupSocket)
        sockets->catchupSocket->touch();

    if (isSyncOnlyNode())
        return;

    if (sockets && sockets->blockProposalSocket)
        sockets->blockProposalSocket->touch();

    LOG(info, "consensus engine exiting: block proposal socket touched");


    LOG(info, "consensus engine exiting: catchup socket touched");

    getSchain()->getCryptoManager()->exitZMQClient();

    LOG(info, "consensus engine exiting: closeAndCleanupAll consensus zmq sockets");

    if (sockets)
        sockets->getConsensusZMQSockets()->closeAndCleanupAll();

}


void Node::registerAgent(Agent *_agent) {
    CHECK_ARGUMENT(_agent);

    LOCK(agentsLock);
    agents.push_back(_agent);
}


void Node::exitCheck() {
    if (exitRequested) {
        BOOST_THROW_EXCEPTION(ExitRequestedException( __CLASS_NAME__ ));
    }
}

void Node::exitOnFatalError(const string &_message) {
    if (exitRequested)
        return;
    exit();

    //    consensusEngine->joinAll();
    auto extFace = consensusEngine->getExtFace();

    if (extFace) {
        extFace->terminateApplication();
    }
    LOG(critical, _message);
}

bool Node::isSgxEnabled() {
    return sgxEnabled;
}
string Node::getEcdsaKeyName() {
    CHECK_STATE(!ecdsaKeyName.empty());
    return ecdsaKeyName;
}
ptr< vector<string> > Node::getEcdsaPublicKeys() {
    CHECK_STATE(ecdsaPublicKeys);
    return ecdsaPublicKeys;
}
string Node::getBlsKeyName() {
    CHECK_STATE(!blsKeyName.empty());
    return blsKeyName;
}
ptr< vector< ptr< vector<string>>>> Node::getBlsPublicKeys() {
    CHECK_STATE(blsPublicKeys);
    return blsPublicKeys;
}
ptr< BLSPublicKey > Node::getBlsPublicKey() {
    CHECK_STATE(blsPublicKey);
    return blsPublicKey;
}
ptr< map< uint64_t, ptr< BLSPublicKey > > > Node::getPreviousBLSPublicKeys() {
    CHECK_STATE(previousBlsPublicKeys);
    return previousBlsPublicKeys;
}
ptr< map< uint64_t, string > > Node::getHistoricECDSAPublicKeys() {
    CHECK_STATE(historicECDSAPublicKeys);
    return historicECDSAPublicKeys;
}
ptr< map< uint64_t, vector< uint64_t > > > Node::getHistoricNodeGroups() {
    CHECK_STATE(historicNodeGroups);
    return historicNodeGroups;
}
bool Node::isInited() const {
    return inited;
}

bool Node::isSyncOnlyNode() const {
    return isSyncNode;
}

