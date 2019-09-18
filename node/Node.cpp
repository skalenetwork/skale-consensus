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

#include "../SkaleCommon.h"
#include "../Log.h"


#include "../exceptions/FatalError.h"
#include "../exceptions/InvalidArgumentException.h"
#include "../exceptions/ParsingException.h"
#include "../exceptions/ExitRequestedException.h"
#include "../thirdparty/json.hpp"



#include "../crypto/bls_include.h"

#include "../libBLS/bls/BLSSignature.h"
#include "../libBLS/bls/BLSPublicKey.h"
#include "../libBLS/bls/BLSPrivateKeyShare.h"
#include "../crypto/SHAHash.h"

#include "../blockproposal/server/BlockProposalServerAgent.h"
#include "../messages/NetworkMessageEnvelope.h"

#include "../chains/Schain.h"
#include "../chains/Schain.h"




#include "../node/NodeInfo.h"
#include "../network/ZMQNetwork.h"
#include "../network/Sockets.h"
#include "../network/TCPServerSocket.h"
#include "../network/ZMQServerSocket.h"

#include "../messages/Message.h"
#include "../catchup/server/CatchupServerAgent.h"
#include "../exceptions/FatalError.h"

#include "../protocols/InstanceGarbageCollectorAgent.h"

#include "../db/BlockDB.h"
#include "../db/RandomDB.h"
#include "../db/CommittedTransactionDB.h"
#include "../db/PriceDB.h"
#include "../db/SigDB.h"





#include "ConsensusEngine.h"
#include "ConsensusInterface.h"
#include "Node.h"

using namespace std;

Node::Node(const nlohmann::json &_cfg, ConsensusEngine *_consensusEngine) {


    this->nodeInfosByIndex = make_shared<map<schain_index, ptr<NodeInfo>>>();
    this->nodeInfosByIP = make_shared<map<ptr<string>, ptr<NodeInfo>, Comparator>>();


    this->consensusEngine = _consensusEngine;
    this->startedServers = false;
    this->startedClients = false;
    this->exitRequested = false;
    this->cfg = _cfg;

    try {
        initParamsFromConfig();
    } catch (...) {
        throw_with_nested(ParsingException("Could not parse params", __CLASS_NAME__));
    }
    initLevelDBs();

    initLogging();

}

void Node::initLevelDBs() {
    string dataDir = *Log::getDataDir();
    string blockDBFilename = dataDir + "/blocks_" + to_string(nodeID) + ".db";
    string randomDBFilename = dataDir + "/randoms_" + to_string(nodeID) + ".db";
    string committedTransactionsDBFilename = dataDir + "/transactions_" + to_string(nodeID) + ".db";
    string signaturesDBFilename = dataDir + "/sigs_" + to_string(nodeID) + ".db";
    string pricesDBFilename = dataDir + "/prices_" + to_string(nodeID) + ".db";


    blockDB = make_shared<BlockDB>(blockDBFilename, getNodeID());
    randomDB = make_shared<RandomDB>(randomDBFilename, getNodeID());
    committedTransactionDB = make_shared<CommittedTransactionDB>(committedTransactionsDBFilename, getNodeID());
    signatureDB = make_shared<SigDB>(signaturesDBFilename, getNodeID());
    priceDB = make_shared<PriceDB>(pricesDBFilename, getNodeID());

}

void Node::initLogging() {
    log = make_shared<Log>(nodeID);

    if (cfg.find("logLevel") != cfg.end()) {
        ptr<string> logLevel = make_shared<string>(cfg.at("logLevel").get<string>());
        log->setGlobalLogLevel(*logLevel);
    }

    for (auto &&item : log->loggers) {
        string category = "logLevel" + item.first;
        if (cfg.find(category) != cfg.end()) {
            ptr<string> logLevel = make_shared<string>(cfg.at(category).get<string>());
            LOG(info, "Setting log level:" + category + ":" + *logLevel);
            log->loggers[item.first]->set_level(Log::logLevelFromString(*logLevel));
        }
    }
}

void Node::initParamsFromConfig() {
    nodeID = cfg.at("nodeID").get<uint64_t>();


    catchupIntervalMS = getParamUint64("catchupIntervalMs", CATCHUP_INTERVAL_MS);

    waitAfterNetworkErrorMs = getParamUint64("waitAfterNetworkErrorMs", WAIT_AFTER_NETWORK_ERROR_MS);

    blockProposalHistorySize = getParamUint64("blockProposalHistorySize", BLOCK_PROPOSAL_HISTORY_SIZE);

    committedTransactionsHistory = getParamUint64("committedTransactionsHistory", COMMITTED_TRANSACTIONS_HISTORY);

    maxCatchupDownloadBytes = getParamUint64("maxCatchupDownloadBytes", MAX_CATCHUP_DOWNLOAD_BYTES);

    maxTransactionsPerBlock = getParamUint64("maxTransactionsPerBlock", MAX_TRANSACTIONS_PER_BLOCK);

    minBlockIntervalMs = getParamUint64("minBlockIntervalMs", MIN_BLOCK_INTERVAL_MS);

    committedBlockStorageSize = getParamUint64("committedBlockStorageSize", COMMITTED_BLOCK_STORAGE_SIZE);

    name = make_shared<string>(cfg.at("nodeName").get<string>());

    bindIP = make_shared<string>(cfg.at("bindIP").get<string>());

    basePort = network_port(cfg.at("basePort").get<int>());

    auto emptyBlockIntervalMsTmp = getParamInt64("emptyBlockIntervalMs", EMPTY_BLOCK_INTERVAL_MS);


    if (emptyBlockIntervalMsTmp < 0) {
        emptyBlockIntervalMs = 100000000000000;
    } else {
        emptyBlockIntervalMs = emptyBlockIntervalMsTmp;
    }

    simulateNetworkWriteDelayMs = getParamInt64("simulateNetworkWriteDelayMs", 0);


}

uint64_t Node::getParamUint64(const string &_paramName, uint64_t paramDefault) {
    try {
        if (cfg.find(_paramName) != cfg.end()) {
            return cfg.at(_paramName).get<uint64_t>();
        } else {
            return paramDefault;
        }
    } catch (...) {
        throw_with_nested(ParsingException("Could not parse param " + _paramName, __CLASS_NAME__));
    }
}

int64_t Node::getParamInt64(const string &_paramName, uint64_t _paramDefault) {
    try {
    if (cfg.find(_paramName) != cfg.end()) {
        return cfg.at(_paramName).get<uint64_t>();
    } else {
        return _paramDefault;
    }

    } catch (...) {
        throw_with_nested(ParsingException("Could not parse param " + _paramName, __CLASS_NAME__));
    }
}


ptr<string> Node::getParamString(const string &_paramName, string& _paramDefault) {
    try {
        if (cfg.find(_paramName) != cfg.end()) {
            return make_shared<string>(cfg.at(_paramName).get<string>());
        } else {
            return make_shared<string>(_paramDefault);
        }

    } catch (...) {
        throw_with_nested(ParsingException("Could not parse param " + _paramName, __CLASS_NAME__));
    }
}


Node::~Node() {

    // sockets means that Node was properly inited
    if (!isExitRequested()) {
        exit();
    }

    cleanLevelDBs();

}

void Node::cleanLevelDBs() {
    blockDB = nullptr;
    randomDB = nullptr;
    committedTransactionDB = nullptr;
    signatureDB = nullptr;
    priceDB = nullptr;
}


node_id Node::getNodeID() const {

    return nodeID;
}


void Node::startServers() {


    initBLSKeys();


    ASSERT(!startedServers);

    LOG(info, "Starting node");

    LOG(info, "Initing sockets");

    this->sockets = make_shared<Sockets>(*this);

    sockets->initSockets(bindIP, (uint16_t) basePort);

    LOG(info, "Constructing servers");

    sChain->constructServers(sockets);

    LOG(info, " Creating consensus network");

    network = make_shared<ZMQNetwork>(*sChain);

    LOG(info, " Starting consensus messaging");

    network->startThreads();

    LOG(info, "Starting schain");

    sChain->startThreads();

    LOG(info, "Releasing server threads");

    releaseGlobalServerBarrier();

}

void Node::initBLSKeys() {
    auto prkStr = consensusEngine->getBlsPrivateKey();
    auto pbkStr1 = consensusEngine->getBlsPublicKey1();
    auto pbkStr2 = consensusEngine->getBlsPublicKey2();
    auto pbkStr3 = consensusEngine->getBlsPublicKey3();
    auto pbkStr4 = consensusEngine->getBlsPublicKey4();

    if (prkStr.size() > 0 && pbkStr1.size() > 0 &&
        pbkStr2.size() > 0 && pbkStr3.size() > 0 &&
        pbkStr4.size() > 0) {
    } else {

        isBLSEnabled = true;
        try {

            prkStr = cfg.at("insecureTestBLSPrivateKey").get<string>();
            pbkStr1 = cfg.at("insecureTestBLSPublicKey1").get<string>();
            pbkStr2 = cfg.at("insecureTestBLSPublicKey2").get<string>();
            pbkStr3 = cfg.at("insecureTestBLSPublicKey3").get<string>();
            pbkStr4 = cfg.at("insecureTestBLSPublicKey4").get<string>();
        } catch (exception &e) {
            isBLSEnabled = false;
            /*throw_with_nested(ParsingException(
                    "Could not find bls key. You need to set it through either skaled or config file\n" +
                    string(e.what()), __CLASS_NAME__));
                    */
        }


        if (prkStr.size() > 0 && pbkStr1.size() > 0 &&
            pbkStr2.size() > 0 && pbkStr3.size() > 0 &&
            pbkStr4.size() > 0) {}
        else {
            isBLSEnabled = false;
            /*
            throw FatalError("Empty bls key. You need to set it through either skaled or config file");
             */
        }
    }

    if (isBLSEnabled) {
        blsPrivateKey = make_shared<BLSPrivateKeyShare>(prkStr, sChain->getTotalSignersCount(), sChain->getRequiredSignersCount());

        auto publicKeyStr = make_shared<vector<string>>();

        publicKeyStr->push_back(pbkStr1);
        publicKeyStr->push_back(pbkStr2);
        publicKeyStr->push_back(pbkStr3);
        publicKeyStr->push_back(pbkStr4);

        blsPublicKey = make_shared<BLSPublicKey>(publicKeyStr, sChain->getTotalSignersCount(),
                sChain->getRequiredSignersCount());
    }
}

void Node::startClients() {
    sChain->healthCheck();
    releaseGlobalClientBarrier();
}


void Node::initSchain(ptr<NodeInfo> _localNodeInfo, const vector<ptr<NodeInfo>> &remoteNodeInfos,
                      ConsensusExtFace *_extFace) {

    try {


        logThreadLocal_ = getLog();

        for (auto &rni : remoteNodeInfos) {
            LOG(debug, "Adding Node Info:" + to_string(rni->getSchainIndex()));
            (*nodeInfosByIndex)[rni->getSchainIndex()] = rni;
            (*nodeInfosByIP)[rni->getBaseIP()] = rni;
            LOG(debug, "Got IP" + *rni->getBaseIP());

        }

        ASSERT(nodeInfosByIndex->size() > 0);
        ASSERT(nodeInfosByIndex->count(1) > 0);

        sChain = make_shared<Schain>(this, _localNodeInfo->getSchainIndex(),
                                     _localNodeInfo->getSchainID(), _extFace);
    } catch (...) {
        throw_with_nested(FatalError(__FUNCTION__, __CLASS_NAME__));
    }

}


void Node::waitOnGlobalServerStartBarrier(Agent *agent) {

    logThreadLocal_ = agent->getSchain()->getNode()->getLog();


    unique_lock<mutex> mlock(threadServerCondMutex);
    while (!startedServers) {
        threadServerConditionVariable.wait(mlock);
    }

}

void Node::releaseGlobalServerBarrier() {
    lock_guard<mutex> lock(threadServerCondMutex);

    startedServers = true;
    threadServerConditionVariable.notify_all();

}


void Node::waitOnGlobalClientStartBarrier(Agent *agent) {

    logThreadLocal_ = agent->getSchain()->getNode()->getLog();


    unique_lock<mutex> mlock(threadClientCondMutex);
    while (!startedClients) {
        threadClientConditionVariable.wait(mlock);
    }

}

void Node::releaseGlobalClientBarrier() {
    lock_guard<mutex> lock(threadClientCondMutex);

//    ASSERT(!startedClients);

    startedClients = true;
    threadClientConditionVariable.notify_all();

}

void Node::exit() {
    if (exitRequested) {
        return;
    }

    exitRequested = true;

    releaseGlobalClientBarrier();
    releaseGlobalServerBarrier();
    LOG(info, "Exit requested");

    closeAllSocketsAndNotifyAllAgentsAndThreads();



}

bool Node::isExitRequested() {
    return exitRequested;
}


void Node::closeAllSocketsAndNotifyAllAgentsAndThreads() {

    getSchain()->getNode()->threadServerConditionVariable.notify_all();

    ASSERT(agents.size() > 0);

    for (auto &&agent : agents) {
        agent->notifyAllConditionVariables();
    }

    if (sockets && sockets->blockProposalSocket)
        sockets->blockProposalSocket->touch();

    if (sockets && sockets->catchupSocket)
        sockets->catchupSocket->touch();

}


void Node::registerAgent( Agent* _agent ) {
    agents.push_back(_agent);
}


ptr<map<schain_index, ptr<NodeInfo>>> Node::getNodeInfosByIndex() const {
    return nodeInfosByIndex;
}


ptr<TransportNetwork> Node::getNetwork() const {
    ASSERT(network);
    return network;
}

nlohmann::json Node::getCfg() const {
    return cfg;
}


Sockets *Node::getSockets() const {
    //ASSERT(sockets);
    return sockets.get();
}

Schain *Node::getSchain() const {
    ASSERT(sChain);
    return sChain.get();
}


ptr<Log> Node::getLog() const {
    ASSERT(log);
    return log;
}


ptr<string> Node::getBindIP() const {
    ASSERT(bindIP != nullptr);
    return bindIP;
}

network_port Node::getBasePort() const {

    ASSERT(basePort > 0);
    return basePort;
}

bool Node::isStarted() const {
    return startedServers;
}


ptr<NodeInfo> Node::getNodeInfoByIndex(schain_index _index) {
    if (nodeInfosByIndex->count(_index) == 0)
        return nullptr;;
    return nodeInfosByIndex->at(_index);
}


ptr<NodeInfo> Node::getNodeInfoByIP(ptr<string> ip) {
    if (nodeInfosByIP->count(ip) == 0)
        return nullptr;
    return nodeInfosByIP->at(ip);
}


ptr<BlockDB> Node::getBlockDB() {
    ASSERT(blockDB != nullptr);
    return blockDB;
}

ptr<RandomDB> Node::getRandomDB() {
    ASSERT(randomDB != nullptr);
    return randomDB;
}

ptr<CommittedTransactionDB> Node::getCommittedTransactionDB() const {
    ASSERT(committedTransactionDB != nullptr);
    return committedTransactionDB;
}


ptr<SigDB> Node::getSignatureDB() const {
    ASSERT(signatureDB != nullptr);
    return signatureDB;
}

ptr<PriceDB> Node::getPriceDB() const {
    ASSERT(priceDB != nullptr)
    return priceDB;
}


void Node::exitCheck() {
    if (exitRequested) {
        BOOST_THROW_EXCEPTION(ExitRequestedException(__CLASS_NAME__));
    }
}

void Node::exitOnFatalError(const string &_message) {
    if (exitRequested)
        return;
    exit();

//    consensusEngine->joinAllThreads();
    auto extFace = consensusEngine->getExtFace();

    if (extFace) {
        extFace->terminateApplication();
    }
    LOG(critical, _message);
}

uint64_t Node::getCatchupIntervalMs() {

    return catchupIntervalMS;
}

uint64_t Node::getWaitAfterNetworkErrorMs() {

    return waitAfterNetworkErrorMs;
}

ConsensusEngine *Node::getConsensusEngine() const {
    return consensusEngine;
}

uint64_t Node::getEmptyBlockIntervalMs() const {
    return emptyBlockIntervalMs;
}

uint64_t Node::getBlockProposalHistorySize() const {
    return blockProposalHistorySize;
}

uint64_t Node::getMaxCatchupDownloadBytes() const {
    return maxCatchupDownloadBytes;
}


uint64_t Node::getMaxTransactionsPerBlock() const {
    return maxTransactionsPerBlock;
}

uint64_t Node::getMinBlockIntervalMs() const {
    return minBlockIntervalMs;
}

uint64_t Node::getCommittedBlockStorageSize() const {
    return committedBlockStorageSize;
}

uint64_t Node::getCommittedTransactionHistoryLimit() const {
    return committedTransactionsHistory;
}

set<node_id> Node::nodeIDs;


ptr<BLSPublicKey> Node::getBlsPublicKey() const {
    if (!blsPublicKey) {
        BOOST_THROW_EXCEPTION(FatalError("Null BLS public key", __CLASS_NAME__));
    }
    return blsPublicKey;
}

ptr<BLSPrivateKeyShare> Node::getBlsPrivateKey() const {
    if (!blsPrivateKey) {
        BOOST_THROW_EXCEPTION(FatalError("Null BLS private key", __CLASS_NAME__));
    }
    return blsPrivateKey;
}

bool Node::isBlsEnabled() const {
    return isBLSEnabled;
}

void Node::setBasePort(const network_port &_basePort) {
    ASSERT(_basePort);
    basePort = _basePort;
}

uint64_t Node::getSimulateNetworkWriteDelayMs() const {
    return simulateNetworkWriteDelayMs;
}

