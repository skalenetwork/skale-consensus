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

    @file Schain.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"
#include "Log.h"

#include "blockfinalize/client/BlockFinalizeDownloader.h"
#include "blockproposal/pusher/BlockProposalClientAgent.h"
#include "blockproposal/server/BlockProposalServerAgent.h"
#include "catchup/client/CatchupClientAgent.h"
#include "catchup/server/CatchupServerAgent.h"
#include "headers/BlockProposalRequestHeader.h"
#include "monitoring/MonitoringAgent.h"
#include "monitoring/TimeoutAgent.h"
#include "monitoring/StuckDetectionAgent.h"
#include "utils/Time.h"


#include "SchainMessageThreadPool.h"
#include "crypto/ConsensusBLSSigShare.h"
#include "crypto/CryptoManager.h"
#include "db/BlockProposalDB.h"
#include "messages/InternalMessageEnvelope.h"
#include "network/ClientSocket.h"
#include "network/IO.h"
#include "node/NodeInfo.h"

#include "protocols/blockconsensus/BlockConsensusAgent.h"
#include "oracle/OracleServerAgent.h"

#include "datastructures/BlockProposal.h"

#include "datastructures/CommittedBlock.h"
#include "datastructures/CommittedBlockList.h"

#include "datastructures/TransactionList.h"
#include "exceptions/ExitRequestedException.h"

#include "pricing/PricingAgent.h"


#include "Schain.h"
#include "db/BlockDB.h"
#include "monitoring/LivelinessMonitor.h"
#include "pendingqueue/TestMessageGeneratorAgent.h"

const ptr<IO> Schain::getIo() const {
    CHECK_STATE(io);
    return io;
}


ptr<PendingTransactionsAgent> Schain::getPendingTransactionsAgent() const {
    CHECK_STATE(pendingTransactionsAgent)
    return pendingTransactionsAgent;
}


ptr<MonitoringAgent> Schain::getMonitoringAgent() const {
    CHECK_STATE(monitoringAgent)
    return monitoringAgent;
}

uint64_t Schain::getStartTimeMs() const {
    return startTimeMs;
}

block_id Schain::getLastCommittedBlockID() const {
    return lastCommittedBlockID.load();
}

ptr<BlockProposal> Schain::getBlockProposal(block_id _blockID, schain_index _schainIndex) {

    MONITOR(__CLASS_NAME__, __FUNCTION__)

    return getNode()->getBlockProposalDB()->getBlockProposal(_blockID, _schainIndex);

}



ptr<CommittedBlock> Schain::getBlock(block_id _blockID) {

    MONITOR(__CLASS_NAME__, __FUNCTION__)

    try {
        return getNode()->getBlockDB()->getBlock(_blockID, getCryptoManager());
    } catch (ExitRequestedException &) { throw; } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


schain_index Schain::getSchainIndex() const {
    return schainIndex;
}


ptr<Node> Schain::getNode() const {
    auto  ret = node.lock();
    if (ret == nullptr) {
        throw ExitRequestedException(__CLASS_NAME__);
    }
    return ret;
}


node_count Schain::getNodeCount() {

    auto count = node_count(getNode()->getNodeInfosByIndex()->size());
    CHECK_STATE(count > 0);
    return count;
}


transaction_count Schain::getMessagesCount() {
    size_t cntMessages = 0;
    { // block
        lock_guard<mutex> lock(messageMutex);
        cntMessages = messageQueue.size();
    } // block
    return transaction_count(cntMessages);
}


schain_id Schain::getSchainID() {
    return schainID;
}

node_id Schain::getNodeIDByIndex(schain_index _index) {
    if (((uint64_t) _index) > (uint64_t) this->getNodeCount()) {
        BOOST_THROW_EXCEPTION(InvalidArgumentException("Index exceeds node count", __CLASS_NAME__));
    }

    auto nodeInfo = this->getNode()->getNodeInfoByIndex(_index);

    return nodeInfo->getNodeID();
}


ptr<BlockConsensusAgent> Schain::getBlockConsensusInstance() {
    CHECK_STATE(blockConsensusInstance != nullptr)
    return blockConsensusInstance;
}

ptr<OracleServerAgent> Schain::getOracleInstance() {
    CHECK_STATE(oracleServer != nullptr)
    return oracleServer;
}


ptr<NodeInfo> Schain::getThisNodeInfo() const {
    CHECK_STATE(thisNodeInfo)
    return thisNodeInfo;
}


ptr<TestMessageGeneratorAgent> Schain::getTestMessageGeneratorAgent() const {
    CHECK_STATE(testMessageGeneratorAgent)
    return testMessageGeneratorAgent;
}

void Schain::setBlockProposerTest(const string &_blockProposerTest) {
    Schain::blockProposerTest = _blockProposerTest;
}


uint64_t Schain::getTotalTransactions() const {
    return totalTransactions;
}

TimeStamp  Schain::getLastCommittedBlockTimeStamp() {
    lock_guard<mutex> l(lastCommittedBlockInfoMutex);
    return lastCommittedBlockTimeStamp;
}


block_id Schain::getBootstrapBlockID() const {
    return bootstrapBlockID.load();
}


void Schain::setHealthCheckFile(uint64_t status) {
    auto engine = getNode()->getConsensusEngine();
    CHECK_STATE(engine);
    string fileName = engine->getHealthCheckDir() + "/HEALTH_CHECK";
    auto id = engine->getEngineID();
    if (id > 1) {
        fileName.append("." + to_string(id));
    }

    ofstream f;
    f.open(fileName, ios::trunc);
    f << status;
    f.close();
}


uint64_t Schain::getTotalSigners() {
    return (uint64_t) getNodeCount();
}


uint64_t Schain::getRequiredSigners() {
    auto count = getNodeCount();
    if (count <= 2) {
        return (uint64_t) count;
    } else {
        return 2 * (uint64_t) count / 3 + 1;
    }
}


u256 Schain::getPriceForBlockId(uint64_t _blockId) {
    CHECK_STATE(pricingAgent);
    return pricingAgent->readPrice(_blockId);
}


string Schain::getBlockProposerTest() const {
    return blockProposerTest;
}

void Schain::setBlockProposerTest(const char *_blockProposerTest) {
    CHECK_ARGUMENT(_blockProposerTest);
    blockProposerTest = _blockProposerTest;
}

ConsensusExtFace *Schain::getExtFace() const {
    return extFace;
}


uint64_t Schain::getMaxExternalBlockProcessingTime() const {
    return maxExternalBlockProcessingTime;;
}

void Schain::joinMonitorAndTimeoutThreads() {
    CHECK_STATE(monitoringAgent);
    monitoringAgent->join();

    if (getNode()->isSyncOnlyNode())
        return;
    CHECK_STATE(timeoutAgent);
    CHECK_STATE(stuckDetectionAgent);

    timeoutAgent->join();
    stuckDetectionAgent->join();
}

 ptr<CryptoManager> Schain::getCryptoManager() const {
    if (!cryptoManager) {
        CHECK_STATE(cryptoManager);
    }
    return cryptoManager;
}

void Schain::createBlockConsensusInstance() {
    blockConsensusInstance = make_shared<BlockConsensusAgent>(*this);
}

void Schain::createOracleInstance() {
    oracleServer = make_shared<OracleServerAgent>(*this);
}

uint64_t Schain::getLastCommitTimeMs() {
    return lastCommitTimeMs;
}


void Schain::initLastCommittedBlockInfo( uint64_t _lastCommittedBlockID,
                                           TimeStamp& _lastCommittedBlockTimeStamp ){

    lock_guard<mutex> l(lastCommittedBlockInfoMutex);
    lastCommittedBlockID = _lastCommittedBlockID;
    lastCommittedBlockTimeStamp = _lastCommittedBlockTimeStamp;
    lastCommitTimeMs = Time::getCurrentTimeMs();

    if (getSchain()->getNode()->isSyncOnlyNode())
        return;
    this->blockConsensusInstance->initFastLedgerAndReplayMessages(lastCommittedBlockID + 1);
}



void Schain::updateLastCommittedBlockInfo( uint64_t _lastCommittedBlockID,
                                   TimeStamp& _lastCommittedBlockTimeStamp,
                                   uint64_t _blockSize){
    lock_guard<mutex> lock(lastCommittedBlockInfoMutex);
    CHECK_STATE(
                _lastCommittedBlockID == lastCommittedBlockID + 1)
    if (_lastCommittedBlockTimeStamp < _lastCommittedBlockTimeStamp) {
        LOG(err, "TimeStamp in the past:"+ lastCommittedBlockTimeStamp.toString() +
            ":"+ _lastCommittedBlockTimeStamp.toString());
    }
    CHECK_STATE(lastCommittedBlockTimeStamp < _lastCommittedBlockTimeStamp);
    auto currentTime = Time::getCurrentTimeMs();
    CHECK_STATE(currentTime >= lastCommitTimeMs);

    lastCommittedBlockID = _lastCommittedBlockID;
    lastCommittedBlockTimeStamp = _lastCommittedBlockTimeStamp;
    lastCommitTimeMs = currentTime;

    blockSizeAverage = (blockSizeAverage * (_lastCommittedBlockID - 1) + _blockSize) / _lastCommittedBlockID;
    blockTimeAverageMs = (currentTime - this->startTimeMs) / (_lastCommittedBlockID - this->bootstrapBlockID);
    if (blockTimeAverageMs == 0)
        blockTimeAverageMs = 1;
    tpsAverage = (blockSizeAverage * 1000 ) / blockTimeAverageMs;
    getRandomForBlockId((uint64_t) lastCommittedBlockID);

    if (getNode()->isSyncOnlyNode())
        return;
    blockConsensusInstance->startNewBlock(lastCommittedBlockID + 1);
}


void Schain::setLastCommittedBlockId( uint64_t _lastCommittedBlockId ) {
    lastCommittedBlockID = _lastCommittedBlockId;
}

const ptr<OracleClient> Schain::getOracleClient() const {
    return oracleClient;
}