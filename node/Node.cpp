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
#include "libBLS/bls/BLSPrivateKeyShare.h"
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

Node::Node(const nlohmann::json &_cfg, ConsensusEngine *_consensusEngine,
           bool _useSGX, ptr<string> _sgxURL,
           ptr<string> _sgxSSLKeyFileFullPath,
           ptr<string> _sgxSSLCertFileFullPath,
           ptr< string > _ecdsaKeyName,
           ptr< vector< string > > _ecdsaPublicKeys, ptr< string > _blsKeyName,
           ptr< vector< ptr< vector< string > > > > _blsPublicKeys,
           ptr< BLSPublicKey > _blsPublicKey) {

    if (_useSGX) {
        CHECK_ARGUMENT(_sgxURL);
        if (_sgxURL->find("https:/") != string::npos) {
            CHECK_ARGUMENT( _sgxSSLKeyFileFullPath );
            CHECK_ARGUMENT( _sgxSSLCertFileFullPath );
        }
        CHECK_ARGUMENT(_ecdsaKeyName && _ecdsaPublicKeys);
        CHECK_ARGUMENT(_blsKeyName && _blsPublicKeys);
        CHECK_ARGUMENT(_blsPublicKey);

        sgxEnabled = true;

        CHECK_STATE(JSONFactory::splitString(*_ecdsaKeyName)->size() == 2);
        CHECK_STATE(JSONFactory::splitString(*_blsKeyName)->size() == 7);

        ecdsaKeyName = _ecdsaKeyName;
        ecdsaPublicKeys = _ecdsaPublicKeys;

        blsKeyName = _blsKeyName;
        blsPublicKeys = _blsPublicKeys;
        blsPublicKey = _blsPublicKey;

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
    auto dbDir = *consensusEngine->getDbDir();
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

}

void Node::initLogging() {
    log = make_shared< SkaleLog >(nodeID, getConsensusEngine());

    if (cfg.find("logLevel") != cfg.end()) {
        ptr<string> logLevel = make_shared<string>(cfg.at("logLevel").get<string>());
        log->setGlobalLogLevel(*logLevel);
    }

    for (auto &&item : log->loggers) {
        string category = "logLevel" + item.first;
        if (cfg.find(category) != cfg.end()) {
            ptr<string> logLevel = make_shared<string>(cfg.at(category).get<string>());
            LOG(info, "Setting log level:" + category + ":" + *logLevel);
            log->loggers[item.first]->set_level( SkaleLog::logLevelFromString(*logLevel));
        }
    }
}


void Node::initParamsFromConfig() {

    auto engine = getConsensusEngine();
    CHECK_STATE(engine);
    auto storageLimits = engine->getStorageLimits();
    CHECK_STATE(storageLimits);

    nodeID = cfg.at("nodeID").get<uint64_t>();
    name = make_shared<string>(cfg.at("nodeName").get<string>());
    bindIP = make_shared<string>(cfg.at("bindIP").get<string>());
    basePort = network_port(cfg.at("basePort").get<int>());

    catchupIntervalMS = getParamUint64("catchupIntervalMs", CATCHUP_INTERVAL_MS);
    monitoringIntervalMS = getParamUint64("monitoringIntervalMs", MONITORING_INTERVAL_MS);
    waitAfterNetworkErrorMs = getParamUint64("waitAfterNetworkErrorMs", WAIT_AFTER_NETWORK_ERROR_MS);
    blockProposalHistorySize = getParamUint64("blockProposalHistorySize", BLOCK_PROPOSAL_HISTORY_SIZE);
    committedTransactionsHistory = getParamUint64("committedTransactionsHistory", COMMITTED_TRANSACTIONS_HISTORY);
    maxCatchupDownloadBytes = getParamUint64("maxCatchupDownloadBytes", MAX_CATCHUP_DOWNLOAD_BYTES);
    maxTransactionsPerBlock = getParamUint64("maxTransactionsPerBlock", MAX_TRANSACTIONS_PER_BLOCK);
    minBlockIntervalMs = getParamUint64("minBlockIntervalMs", MIN_BLOCK_INTERVAL_MS);


    blockDBSize = getParamUint64("blockDBSize", storageLimits->getBlockDbSize());
    proposalHashDBSize = getParamUint64("proposalHashDBSize", storageLimits->getProposalHashDbSize() );
    proposalVectorDBSize = getParamUint64("proposalVectorDBSize", storageLimits->getProposalVectorDbSize());
    outgoingMsgDBSize = getParamUint64("outgoingMsgDBSize", storageLimits->getOutgoingMsgDbSize());
    incomingMsgDBSize = getParamUint64("incomingMsgDBSize", storageLimits->getIncomingMsgDbSize());
    consensusStateDBSize = getParamUint64("consensusStateDBSize", storageLimits->getConsensusStateDbSize());

    blockSigShareDBSize = getParamUint64("blockSigShareDBSize", storageLimits->getBlockSigShareDbSize());
    daSigShareDBSize = getParamUint64("daSigShareDBSize", storageLimits->getDaSigShareDbSize());
    daProofDBSize = getParamUint64("daProofDBSize", storageLimits->getDaProofDbSize());
    randomDBSize = getParamUint64("randomDBSize", storageLimits->getRandomDbSize());
    priceDBSize = getParamUint64("priceDBSize", storageLimits->getPriceDbSize());
    blockProposalDBSize = getParamUint64("blockProposalDBSize", storageLimits->getBlockProposalDbSize());


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

    ASSERT(!startedServers);

    LOG(info, "Starting node");

    LOG(trace, "Initing sockets");

    this->sockets = make_shared<Sockets>(*this);

    sockets->initSockets(bindIP, (uint16_t) basePort);

    LOG(trace, "Constructing servers");

    sChain->constructServers(sockets);

    LOG(trace, " Creating consensus network");

    network = make_shared<ZMQNetwork>(*sChain);

    LOG(trace, " Starting consensus messaging");

    network->startThreads();

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


void Node::setNodeInfo(ptr<NodeInfo> _nodeInfo) {

    CHECK_ARGUMENT(_nodeInfo);
    (*nodeInfosByIndex)[(uint64_t)_nodeInfo->getSchainIndex()] =  _nodeInfo;
    (*nodeInfosById)[(uint64_t ) _nodeInfo->getNodeID()] = _nodeInfo;
}

void Node::setSchain(ptr<Schain> _schain) {
    CHECK_STATE(sChain == nullptr);
    this->sChain = _schain;
    initLevelDBs();

}

void Node::initSchain(ptr<Node> _node, ptr<NodeInfo> _localNodeInfo, const vector<ptr<NodeInfo> > &remoteNodeInfos,
                      ConsensusExtFace *_extFace) {
    try {
        logThreadLocal_ = _node->getLog();

        for (auto &rni : remoteNodeInfos) {
            LOG(debug, "Adding Node Info:" + to_string(rni->getSchainIndex()));
            _node->setNodeInfo(rni);
            LOG(debug, "Got IP" + *rni->getBaseIP());
        }


        _node->testNodeInfos();




        auto sChain = make_shared<Schain>(
                _node, _localNodeInfo->getSchainIndex(), _localNodeInfo->getSchainID(), _extFace);

        _node->setSchain(sChain);

        sChain->createBlockConsensusInstance();

    } catch (...) {
        throw_with_nested(FatalError(__FUNCTION__, __CLASS_NAME__));
    }

}


void Node::waitOnGlobalServerStartBarrier(Agent * _agent ) {
    CHECK_ARGUMENT( _agent );

    logThreadLocal_ = _agent->getSchain()->getNode()->getLog();


    std::unique_lock<std::mutex> mlock(threadServerCondMutex);
    while (!startedServers) {
        threadServerConditionVariable.wait(mlock);
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
        threadClientConditionVariable.wait_for( mlock, std::chrono::milliseconds( 5000 ) ); // threadClientConditionVariable.wait(mlock);
    }
}


void Node::releaseGlobalClientBarrier() {

    RETURN_IF_PREVIOUSLY_CALLED(startedClients)

    std::lock_guard<std::mutex> lock(threadClientCondMutex);

    threadClientConditionVariable.notify_all();
}

void Node::exit() {

    RETURN_IF_PREVIOUSLY_CALLED(exitRequested);

    releaseGlobalClientBarrier();
    releaseGlobalServerBarrier();
    LOG(info, "Exit requested");

    closeAllSocketsAndNotifyAllAgentsAndThreads();

}


void Node::closeAllSocketsAndNotifyAllAgentsAndThreads() {
    getSchain()->getNode()->threadServerConditionVariable.notify_all();

    CHECK_STATE(agents.size() > 0);

    for (auto &&agent : agents) {
        agent->notifyAllConditionVariables();
    }

    if (sockets && sockets->blockProposalSocket)
        sockets->blockProposalSocket->touch();

    if (sockets && sockets->catchupSocket)
        sockets->catchupSocket->touch();

    if (sockets)
        sockets->getConsensusZMQSockets()->closeAndCleanupAll();

}


void Node::registerAgent(Agent *_agent) {
    CHECK_ARGUMENT(_agent);
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
ptr< string > Node::getEcdsaKeyName() {
    CHECK_STATE(ecdsaKeyName);
    return ecdsaKeyName;
}
ptr< vector< string > > Node::getEcdsaPublicKeys() {
    CHECK_STATE(ecdsaPublicKeys);
    return ecdsaPublicKeys;
}
ptr< string > Node::getBlsKeyName() {
    CHECK_STATE(blsKeyName);
    return blsKeyName;
}
ptr< vector< ptr< vector< string > > > > Node::getBlsPublicKeys() {
    CHECK_STATE(blsPublicKeys);
    return blsPublicKeys;
}
ptr< BLSPublicKey > Node::getBlsPublicKey() {
    CHECK_STATE(blsPublicKey);
    return blsPublicKey;
}
