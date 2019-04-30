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

    @file Node.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../exceptions/InvalidArgumentException.h"

#include "../thirdparty/json.hpp"
#include "leveldb/db.h"


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


#include "../blockproposal/server/BlockProposalServerAgent.h"
#include "../messages/NetworkMessageEnvelope.h"
#include "../chains/Schain.h"
#include "../chains/Schain.h"
#include "../exceptions/ExitRequestedException.h"


#include "../node/NodeInfo.h"
#include "../network/ZMQNetwork.h"
#include "../network/Sockets.h"
#include "../network/TCPServerSocket.h"
#include "../network/ZMQServerSocket.h"

#include "../crypto/BLSPublicKey.h"
#include "../crypto/BLSPrivateKey.h"
#include "../crypto/SHAHash.h"


#include "../messages/Message.h"
#include "../catchup/server/CatchupServerAgent.h"
#include "../exceptions/FatalError.h"

#include "../protocols/InstanceGarbageCollectorAgent.h"
#include "ConsensusEngine.h"
#include "ConsensusInterface.h"
#include "Node.h"
#include "../exceptions/ParsingException.h"

using namespace std;

Node::Node(const nlohmann::json &_cfg, ConsensusEngine *_consensusEngine) {


    this->consensusEngine = _consensusEngine;
    this->startedServers = false;
    this->startedClients = false;
    this->exitRequested = false;
    this->cfg = _cfg;

    initParamsFromConfig();

    initLevelDBs();

    initLogging();

}

void Node::initLevelDBs() {
    string dataDir = *Log::getDataDir();
    string blockDBFilename = dataDir + "/blocks_" + to_string(nodeID) + ".db";
    string committedTransactionsDBFilename = dataDir + "/transactions_" + to_string(nodeID) + ".db";
    string signaturesDBFilename = dataDir + "/sigs_" + to_string(nodeID) + ".db";


    blocksDB = make_shared<LevelDB>(blockDBFilename);
    committedTransactionsDB = make_shared<LevelDB>(committedTransactionsDBFilename);
    signaturesDB = make_shared<LevelDB>(signaturesDBFilename);

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

    basePort = network_port(cfg.at("basePort").get<uint16_t>());


    auto emptyBlockIntervalMsTmp = getParamInt64("emptyBlockIntervalMs", EMPTY_BLOCK_INTERVAL_MS);

    if (emptyBlockIntervalMsTmp < 0) {
        emptyBlockIntervalMs = 100000000000000;
    } else {
        emptyBlockIntervalMs = emptyBlockIntervalMsTmp;
    }


}

uint64_t Node::getParamUint64(const string &paramName, uint64_t paramDefault) {
    if (cfg.find(paramName) != cfg.end()) {
        return cfg.at(paramName).get<uint64_t>();
    } else {
        return paramDefault;
    }
}

int64_t Node::getParamInt64(const string &paramName, uint64_t paramDefault) {
    if (cfg.find(paramName) != cfg.end()) {
        return cfg.at(paramName).get<uint64_t>();
    } else {
        return paramDefault;
    }
}


Node::~Node() {

    if (!isExitRequested()) {
        exit();
    }

    cleanLevelDBs();

}

void Node::cleanLevelDBs() {
    blocksDB = nullptr;
    committedTransactionsDB = nullptr;
    signaturesDB = nullptr;
}


node_id Node::getNodeID() const {
    return nodeID;
}


void Node::start() {


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
        } catch (std::exception &e) {
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
        blsPrivateKey = make_shared<BLSPrivateKey>(prkStr, sChain->getNodeCount());
        blsPublicKey = make_shared<BLSPublicKey>(pbkStr1, pbkStr2, pbkStr3, pbkStr4, sChain->getNodeCount());
    }


    ASSERT(!startedServers);

    LOG(info, "Starting node");

    LOG(info, "Initing sockets");

    this->sockets = make_shared<Sockets>(*this);

    sockets->initSockets(bindIP, (uint16_t) basePort);


    LOG(info, "Creating block proposal server");

    blockProposalServerAgent = make_shared<BlockProposalServerAgent>(*sChain, sockets->blockProposalSocket);

    LOG(info, "Creating catchup server");

    catchupServerAgent = make_shared<CatchupServerAgent>(*sChain, sockets->catchupSocket);


    LOG(info, " Creating consensus network");


    network = make_shared<ZMQNetwork>(*sChain);

    LOG(info, " Starting consensus messaging");

    network->startThreads();

    LOG(info, "Starting schain");

    sChain->startThreads();

    LOG(info, "Releasing all threads");

    releaseGlobalServerBarrier();

}

void Node::startClients() {

    sChain->healthCheck();

    releaseGlobalClientBarrier();

    LOG(info, "Started node");
}


void Node::initSchain(ptr<NodeInfo> _localNodeInfo, const vector<ptr<NodeInfo>> &remoteNodeInfos,
                      ConsensusExtFace *_extFace) {


    logThreadLocal_ = getLog();

    for (auto &rni : remoteNodeInfos) {
        LOG(debug, "Adding Node Info:" + to_string(rni->getSchainIndex()));
        nodeInfosByIndex[rni->getSchainIndex()] = rni;
        nodeInfosByIP[rni->getBaseIP()] = rni;
        LOG(debug, "Got IP" + *rni->getBaseIP());

    }

    ASSERT(nodeInfosByIndex.size() > 0);
    ASSERT(nodeInfosByIndex.count(0) > 0);

    sChain = make_shared<Schain>(*this, _localNodeInfo->getSchainIndex(),
                                 _localNodeInfo->getSchainID(), _extFace);

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

    releaseGlobalClientBarrier();
    releaseGlobalServerBarrier();
    LOG(info, "Exit requested");
    exitRequested = true;
    closeAllSocketsAndNotifyAllAgentsAndThreads();

}

bool Node::isExitRequested() {
    return exitRequested;
}


void Node::closeAllSocketsAndNotifyAllAgentsAndThreads() {

    getSchain()->getNode()->threadServerConditionVariable.notify_all();

    for (auto &&agent : agents) {
        agent->notifyAllConditionVariables();
    }

    if (sockets->blockProposalSocket)
        sockets->blockProposalSocket->touch();

    if (sockets->catchupSocket)
        sockets->catchupSocket->touch();

}


vector<Agent *> &Node::getAgents() {
    return agents;
}


const map<schain_index, ptr<NodeInfo>> &Node::getNodeInfosByIndex() const {

    return nodeInfosByIndex;
}


const ptr<TransportNetwork> &Node::getNetwork() const {
    ASSERT(network);
    return network;
}

const nlohmann::json &Node::getCfg() const {
    return cfg;
}


Sockets *Node::getSockets() const {
    ASSERT(sockets);
    return sockets.get();
}

Schain *Node::getSchain() const {
    ASSERT(sChain);
    return sChain.get();
}


const ptr<Log> &Node::getLog() const {
    ASSERT(log);
    return log;
}


const ptr<string> &Node::getBindIP() const {
    return bindIP;
}

const network_port &Node::getBasePort() const {
    return basePort;
}

bool Node::isStarted() const {
    return startedServers;
}


ptr<NodeInfo> Node::getNodeInfoByIndex(schain_index _index) {
    if (nodeInfosByIndex.count(_index) == 0)
        return nullptr;;
    return nodeInfosByIndex[_index];
}


ptr<NodeInfo> Node::getNodeInfoByIP(ptr<string> ip) {
    if (nodeInfosByIP.count(ip) == 0)
        return nullptr;;
    return nodeInfosByIP[ip];
}


ptr<LevelDB> Node::getBlocksDB() {
    assert(blocksDB);
    return blocksDB;
}

ptr<LevelDB> Node::getCommittedTransactionsDB() const {
    assert(committedTransactionsDB);
    return committedTransactionsDB;
}


ptr<LevelDB> Node::getSignaturesDB() const {
    assert(signaturesDB);
    return signaturesDB;
}

void Node::exitCheck() {
    if (exitRequested) {
        throw ExitRequestedException();
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



const ptr<BLSPublicKey> &Node::getBlsPublicKey() const {
    if (!blsPublicKey) {
        BOOST_THROW_EXCEPTION(FatalError("Null BLS public key", __CLASS_NAME__));
    }
    return blsPublicKey;
}

const ptr<BLSPrivateKey> &Node::getBlsPrivateKey() const {
    if (!blsPrivateKey) {
        BOOST_THROW_EXCEPTION(FatalError("Null BLS private key", __CLASS_NAME__));
    }
    return blsPrivateKey;
}

bool Node::isBlsEnabled() const {
    return isBLSEnabled;
}
