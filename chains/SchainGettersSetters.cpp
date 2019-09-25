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

#include "../Log.h"
#include "../SkaleCommon.h"
#include "../exceptions/FatalError.h"
#include "../exceptions/InvalidArgumentException.h"

#include "../thirdparty/json.hpp"

#include "../abstracttcpserver/ConnectionStatus.h"
#include "../node/ConsensusEngine.h"

#include <unordered_set>

#include "leveldb/db.h"

#include "../node/ConsensusEngine.h"
#include "../node/Node.h"

#include "../blockproposal/pusher/BlockProposalClientAgent.h"
#include "../headers/BlockProposalHeader.h"
#include "../pendingqueue/PendingTransactionsAgent.h"

#include "../blockfinalize/client/BlockFinalizeDownloader.h"
#include "../blockproposal/server/BlockProposalServerAgent.h"
#include "../catchup/client/CatchupClientAgent.h"
#include "../catchup/server/CatchupServerAgent.h"
#include "../monitoring/MonitoringAgent.h"


#include "../crypto/ConsensusBLSSigShare.h"
#include "../exceptions/EngineInitException.h"
#include "../exceptions/ParsingException.h"
#include "../messages/InternalMessageEnvelope.h"
#include "../messages/Message.h"
#include "../messages/MessageEnvelope.h"
#include "../messages/NetworkMessageEnvelope.h"
#include "../node/NodeInfo.h"


#include "../blockfinalize/received/ReceivedBlockSigSharesDatabase.h"
#include "../blockproposal/received/ReceivedBlockProposalsDatabase.h"
#include "../network/Sockets.h"
#include "../protocols/ProtocolInstance.h"
#include "../protocols/blockconsensus/BlockConsensusAgent.h"

#include "../network/ClientSocket.h"
#include "../network/IO.h"
#include "../network/ZMQServerSocket.h"
#include "SchainMessageThreadPool.h"

#include "../crypto/SHAHash.h"
#include "../datastructures/BlockProposal.h"
#include "../datastructures/BlockProposalSet.h"
#include "../datastructures/CommittedBlock.h"
#include "../datastructures/CommittedBlockList.h"
#include "../datastructures/MyBlockProposal.h"
#include "../datastructures/ReceivedBlockProposal.h"
#include "../datastructures/Transaction.h"
#include "../datastructures/TransactionList.h"
#include "../exceptions/ExitRequestedException.h"
#include "../messages/ConsensusProposalMessage.h"

#include "../exceptions/FatalError.h"


#include "../pricing/PricingAgent.h"


#include "../crypto/bls_include.h"
#include "../db/BlockDB.h"
#include "../db/LevelDB.h"
#include "../pendingqueue/TestMessageGeneratorAgent.h"
#include "SchainTest.h"


#include "../libBLS/bls/BLSPrivateKeyShare.h"
#include "../monitoring/LivelinessMonitor.h"
#include "Schain.h"

uint64_t Schain::getHighResolutionTime() {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
}


const ptr<IO> Schain::getIo() const {
    CHECK_STATE(io != nullptr);
    return io;
}


ptr<PendingTransactionsAgent> Schain::getPendingTransactionsAgent() const {
    CHECK_STATE(pendingTransactionsAgent != nullptr)
    return pendingTransactionsAgent;
}


ptr<MonitoringAgent> Schain::getMonitoringAgent() const {
    CHECK_STATE(monitoringAgent != nullptr)
    return monitoringAgent;
}

uint64_t Schain::getStartTimeMs() const {
    return startTimeMs;
}

const block_id Schain::getLastCommittedBlockID() const {
    return block_id(lastCommittedBlockID.load());
}

ptr<BlockProposal> Schain::getBlockProposal(block_id _blockID, schain_index _schainIndex) {

    MONITOR(__CLASS_NAME__, __FUNCTION__)

    return blockProposalsDatabase->getBlockProposal(_blockID, _schainIndex);

}

ptr<CommittedBlock> Schain::getCachedBlock(block_id _blockID) {
    std::lock_guard<std::recursive_mutex> aLock(getMainMutex());

    if (blocks.count(_blockID > 0)) {
        return blocks.at(_blockID);
    } else {
        return nullptr;
    }
}

ptr<CommittedBlock> Schain::getBlock(block_id _blockID) {

    MONITOR(__CLASS_NAME__, __FUNCTION__)

    std::lock_guard<std::recursive_mutex> aLock(getMainMutex());

    auto block = getCachedBlock(_blockID);

    if (block)
        return block;


    auto serializedBlock = getNode()->getBlockDB()->getSerializedBlock(_blockID);

    if (serializedBlock == nullptr) {
        return nullptr;
    }

    return CommittedBlock::deserialize(serializedBlock);
}


ptr<vector<uint8_t> > Schain::getSerializedBlock(uint64_t i) const {

    MONITOR(__CLASS_NAME__, __FUNCTION__)

    auto block = sChain->getCachedBlock(i);


    if (block) {
        return block->getSerialized();
    } else {
        return getNode()->getBlockDB()->getSerializedBlock(i);
    }
}


schain_index Schain::getSchainIndex() const {
    return schainIndex;
}


Node *Schain::getNode() const {
    CHECK_STATE(node != nullptr);
    return node;
}


node_count Schain::getNodeCount() {
    auto count = node_count(getNode()->getNodeInfosByIndex()->size());
    ASSERT(count > 0);
    return count;
}


transaction_count Schain::getMessagesCount() {
    std::lock_guard<std::recursive_mutex> aLock(getMainMutex());

    return transaction_count(messageQueue.size());
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


ptr<NodeInfo> Schain::getThisNodeInfo() const {
    CHECK_STATE(thisNodeInfo != nullptr)
    return thisNodeInfo;
}


ptr<TestMessageGeneratorAgent> Schain::getTestMessageGeneratorAgent() const {
    CHECK_STATE(testMessageGeneratorAgent != nullptr)
    return testMessageGeneratorAgent;
}

void Schain::setBlockProposerTest(const string &blockProposerTest) {
    Schain::blockProposerTest = make_shared<string>(blockProposerTest);
}


schain_id Schain::getSchainID() const {
    return schainID;
}


uint64_t Schain::getTotalTransactions() const {
    return totalTransactions;
}

uint64_t Schain::getLastCommittedBlockTimeStamp() {
    return committedBlockTimeStamp;
}


block_id Schain::getBootstrapBlockID() const {
    return bootstrapBlockID.load();
}


void Schain::setHealthCheckFile(uint64_t status) {
    string fileName = Log::getDataDir()->append("/HEALTH_CHECK");


    ofstream f;
    f.open(fileName, ios::trunc);
    f << status;
    f.close();
}


size_t Schain::getTotalSignersCount() {
    return (size_t) (uint64_t) getNodeCount();
}


size_t Schain::getRequiredSignersCount() {
    auto count = getNodeCount();
    if (count <= 2) {
        return (uint64_t) count;
    } else {
        return 2 * (uint64_t) count / 3 + 1;
    }
}

u256 Schain::getPriceForBlockId(uint64_t _blockId) {
    CHECK_STATE(pricingAgent != nullptr);
    return pricingAgent->readPrice(_blockId);
}


const ptr<string> Schain::getBlockProposerTest() const {
    return blockProposerTest;
}

void Schain::setBlockProposerTest(const char *_blockProposerTest) {
    blockProposerTest = make_shared<string>(_blockProposerTest);
}

ConsensusExtFace * Schain::getExtFace() const {
    return extFace;
}



uint64_t Schain::getMaxExternalBlockProcessingTime() const {
    return maxExternalBlockProcessingTime;;
}

