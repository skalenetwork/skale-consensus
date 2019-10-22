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

    @file NodeGettersSetters.cpp
    @author Stan Kladko
    @date 2018
*/

#include "leveldb/db.h"

#include "../Log.h"
#include "../SkaleCommon.h"


#include "../exceptions/ExitRequestedException.h"
#include "../exceptions/FatalError.h"
#include "../exceptions/InvalidArgumentException.h"
#include "../exceptions/ParsingException.h"
#include "../thirdparty/json.hpp"

#include "../chains/TestConfig.h"

#include "../crypto/bls_include.h"

#include "../crypto/SHAHash.h"
#include "../libBLS/bls/BLSPrivateKeyShare.h"
#include "../libBLS/bls/BLSPublicKey.h"
#include "../libBLS/bls/BLSSignature.h"

#include "../blockproposal/server/BlockProposalServerAgent.h"
#include "../messages/NetworkMessageEnvelope.h"

#include "../chains/Schain.h"


#include "../network/Sockets.h"
#include "../network/TCPServerSocket.h"
#include "../network/ZMQNetwork.h"
#include "../network/ZMQServerSocket.h"
#include "../node/NodeInfo.h"

#include "../catchup/server/CatchupServerAgent.h"
#include "../exceptions/FatalError.h"
#include "../messages/Message.h"

#include "../protocols/InstanceGarbageCollectorAgent.h"

#include "../db/BlockDB.h"
#include "../db/CommittedTransactionDB.h"
#include "../db/PriceDB.h"
#include "../db/RandomDB.h"
#include "../db/SigDB.h"


#include "ConsensusEngine.h"
#include "ConsensusInterface.h"
#include "Node.h"

using namespace std;



uint64_t Node::getParamUint64(const string &_paramName, uint64_t paramDefault) {
    try {
        if (cfg.find(_paramName) != cfg.end()) {
            return cfg.at(_paramName).get<uint64_t>();
        } else {
            return paramDefault;
        }
    } catch (...) {
        throw_with_nested(
                ParsingException("Could not parse param " + _paramName, __CLASS_NAME__));
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
        throw_with_nested(
                ParsingException("Could not parse param " + _paramName, __CLASS_NAME__));
    }
}


ptr<string> Node::getParamString(const string &_paramName, string &_paramDefault) {
    try {
        if (cfg.find(_paramName) != cfg.end()) {
            return make_shared<string>(cfg.at(_paramName).get<string>());
        } else {
            return make_shared<string>(_paramDefault);
        }

    } catch (...) {
        throw_with_nested(
                ParsingException("Could not parse param " + _paramName, __CLASS_NAME__));
    }
}





node_id Node::getNodeID() const {
    return nodeID;
}





const ptr<ProposalHashDB> &Node::getProposalHashDb() const {
    return proposalHashDB;
}



ptr<map<schain_index, ptr<NodeInfo> > > Node::getNodeInfosByIndex() const {
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
    // ASSERT(sockets);
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


uint64_t Node::getCatchupIntervalMs() {
    return catchupIntervalMS;
}

uint64_t Node::getMonitoringIntervalMs() {
    return monitoringIntervalMS;
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


void Node::setBasePort(const network_port &_basePort) {
    CHECK_ARGUMENT(_basePort > 0);
    basePort = _basePort;
}

uint64_t Node::getSimulateNetworkWriteDelayMs() const {
    return simulateNetworkWriteDelayMs;
}

const ptr<TestConfig> &Node::getTestConfig() const {
    CHECK_STATE(testConfig != nullptr)
    return testConfig;
}

bool Node::isExitRequested() {
    return exitRequested;
}


bool Node::isBlsEnabled() const {
    return isBLSEnabled;
}

bool Node::isStarted() const {
    return startedServers;
}