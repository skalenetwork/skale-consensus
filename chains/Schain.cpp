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

#include <unordered_set>
#include "leveldb/db.h"

#include "../Log.h"
#include "../SkaleCommon.h"
#include "../exceptions/FatalError.h"
#include "../exceptions/InvalidArgumentException.h"
#include "../thirdparty/json.hpp"


#include "../utils/Time.h"
#include "../abstracttcpserver/ConnectionStatus.h"
#include "../exceptions/InvalidStateException.h"
#include "../node/ConsensusEngine.h"
#include "../node/ConsensusEngine.h"
#include "../node/Node.h"
#include "../blockproposal/pusher/BlockProposalClientAgent.h"
#include "../headers/BlockProposalRequestHeader.h"
#include "../pendingqueue/PendingTransactionsAgent.h"

#include "../blockfinalize/client/BlockFinalizeDownloader.h"
#include "../blockproposal/server/BlockProposalServerAgent.h"
#include "../catchup/client/CatchupClientAgent.h"
#include "../catchup/server/CatchupServerAgent.h"
#include "../crypto/ThresholdSignature.h"
#include "../monitoring/MonitoringAgent.h"
#include "../crypto/ConsensusBLSSigShare.h"
#include "../exceptions/EngineInitException.h"
#include "../exceptions/ParsingException.h"
#include "../messages/InternalMessageEnvelope.h"
#include "../messages/Message.h"
#include "../messages/MessageEnvelope.h"
#include "../messages/NetworkMessageEnvelope.h"
#include "../node/NodeInfo.h"
#include "../db/ReceivedBlockProposalsDB.h"
#include "../db/ReceivedDASigSharesDB.h"
#include "../network/Sockets.h"
#include "../protocols/ProtocolInstance.h"
#include "../protocols/blockconsensus/BlockConsensusAgent.h"
#include "../network/ClientSocket.h"
#include "../network/IO.h"
#include "../network/ZMQServerSocket.h"
#include "../crypto/SHAHash.h"
#include "../datastructures/BlockProposal.h"
#include "../datastructures/BlockProposalSet.h"
#include "../datastructures/CommittedBlock.h"
#include "../datastructures/CommittedBlockList.h"
#include "../datastructures/MyBlockProposal.h"
#include "../datastructures/ReceivedBlockProposal.h"
#include "../datastructures/Transaction.h"
#include "../datastructures/TransactionList.h"
#include "../datastructures/DAProof.h"
#include "../exceptions/ExitRequestedException.h"
#include "../messages/ConsensusProposalMessage.h"
#include "../exceptions/FatalError.h"
#include "../pricing/PricingAgent.h"


#include "../crypto/bls_include.h"
#include "../crypto/ThresholdSigShare.h"
#include "../db/BlockDB.h"
#include "../db/LevelDB.h"
#include "../db/ProposalHashDB.h"
#include "../pendingqueue/TestMessageGeneratorAgent.h"
#include "SchainTest.h"
#include "../libBLS/bls/BLSPrivateKeyShare.h"
#include "../monitoring/LivelinessMonitor.h"
#include "../crypto/CryptoManager.h"
#include "SchainMessageThreadPool.h"
#include "TestConfig.h"
#include "Schain.h"


void Schain::postMessage(ptr<MessageEnvelope> m) {


    checkForExit();


    lock_guard<mutex> lock(messageMutex);

    ASSERT(m);
    ASSERT((uint64_t) m->getMessage()->getBlockId() != 0);


    messageQueue.push(m);
    messageCond.notify_all();
}


void Schain::messageThreadProcessingLoop(Schain *s) {
    ASSERT(s);


    s->waitOnGlobalStartBarrier();


    try {
        s->startTimeMs = Time::getCurrentTimeMs();

        logThreadLocal_ = s->getNode()->getLog();

        queue<ptr<MessageEnvelope> > newQueue;

        while (!s->getNode()->isExitRequested()) {
            {


                unique_lock<mutex> mlock(s->messageMutex);
                while (s->messageQueue.empty()) {
                    s->messageCond.wait(mlock);
                    if (s->getNode()->isExitRequested()) {
                        s->getNode()->getSockets()->consensusZMQSocket->closeSend();
                        return;
                    }
                }

                newQueue = s->messageQueue;


                while (!s->messageQueue.empty()) {
                    s->messageQueue.pop();
                }
            }


            while (!newQueue.empty()) {
                ptr<MessageEnvelope> m = newQueue.front();


                ASSERT((uint64_t) m->getMessage()->getBlockId() != 0);

                try {
                    s->getBlockConsensusInstance()->routeAndProcessMessage(m);
                } catch (Exception &e) {
                    if (s->getNode()->isExitRequested()) {
                        s->getNode()->getSockets()->consensusZMQSocket->closeSend();
                        return;
                    }
                    Exception::logNested(e);
                }

                newQueue.pop();
            }
        }


        s->getNode()->getSockets()->consensusZMQSocket->closeSend();
    } catch (FatalError *e) {
        s->getNode()->exitOnFatalError(e->getMessage());
    }
}


void Schain::startThreads() {
    this->consensusMessageThreadPool->startService();
}


Schain::Schain(weak_ptr<Node> _node, schain_index _schainIndex, const schain_id &_schainID, ConsensusExtFace *_extFace)
        : Agent(
        *this, true, true), totalTransactions(0), extFace(_extFace), schainID(_schainID), consensusMessageThreadPool(
        new SchainMessageThreadPool(this)), node(_node), schainIndex(_schainIndex) {

    // construct monitoring agent early
    monitoringAgent = make_shared<MonitoringAgent>(*this);
    maxExternalBlockProcessingTime = std::max(2 * getNode()->getEmptyBlockIntervalMs(), (uint64_t) 3000);

    MONITOR(__CLASS_NAME__, __FUNCTION__)

    ASSERT(schainIndex > 0);

    try {

        this->io = make_shared<IO>(this);

        ASSERT(getNode()->getNodeInfosByIndex()->size() > 0);

        for (auto const &iterator : *getNode()->getNodeInfosByIndex()) {
            if (iterator.second->getNodeID() == getNode()->getNodeID()) {
                ASSERT(thisNodeInfo == nullptr && iterator.second != nullptr);
                thisNodeInfo = iterator.second;
            }
        }

        if (thisNodeInfo == nullptr) {
            BOOST_THROW_EXCEPTION(EngineInitException(
                                          "Schain: " + to_string((uint64_t) getSchainID()) +
                                          " does not include current node with IP " + *getNode()->getBindIP() +
                                          "and node id " + to_string(getNode()->getNodeID()), __CLASS_NAME__));
        }

        ASSERT(getNodeCount() > 0);

        constructChildAgents();

        string x = SchainTest::NONE;

        blockProposerTest = make_shared<string>(x);

        getNode()->registerAgent(this);


    } catch (ExitRequestedException &) { throw; } catch (...) {
        throw_with_nested(FatalError(__FUNCTION__, __CLASS_NAME__));
    }


}


void Schain::constructChildAgents() {

    MONITOR(__CLASS_NAME__, __FUNCTION__)

    try {
        LOCK(m)
        pendingTransactionsAgent = make_shared<PendingTransactionsAgent>(*this);
        blockProposalClient = make_shared<BlockProposalClientAgent>(*this);
        catchupClientAgent = make_shared<CatchupClientAgent>(*this);
        blockConsensusInstance = make_shared<BlockConsensusAgent>(*this);
        blockProposalsDatabase = make_shared<ReceivedBlockProposalsDB>(*this);
        receivedDASigSharesDatabase = make_shared<ReceivedDASigSharesDB>(*this);
        testMessageGeneratorAgent = make_shared<TestMessageGeneratorAgent>(*this);
        pricingAgent = make_shared<PricingAgent>(*this);
        cryptoManager = make_shared<CryptoManager>(*this);

    } catch (...) {
        throw_with_nested(FatalError(__FUNCTION__, __CLASS_NAME__));
    }
}


void Schain::blockCommitsArrivedThroughCatchup(ptr<CommittedBlockList> _blocks) {
    ASSERT(_blocks);

    auto b = _blocks->getBlocks();

    ASSERT(b);

    if (b->size() == 0) {
        return;
    }


    LOCK(m)


    atomic<uint64_t> committedIDOld = (uint64_t ) getLastCommittedBlockID();

    uint64_t previosBlockTimeStamp = 0;
    uint64_t previosBlockTimeStampMs = 0;


    ASSERT(b->at(0)->getBlockID() <= (uint64_t) getLastCommittedBlockID() + 1);

    for (size_t i = 0; i < b->size(); i++) {
        auto t = b->at(i);

        if ((uint64_t ) t->getBlockID() > getLastCommittedBlockID()) {
            processCommittedBlock(t);
            previosBlockTimeStamp = t->getTimeStamp();
            previosBlockTimeStampMs = t->getTimeStampMs();
        }
    }

    if (committedIDOld < getLastCommittedBlockID()) {
        LOG(info, "BLOCK_CATCHUP: " + to_string(getLastCommittedBlockID() - committedIDOld) + " BLOCKS");
        proposeNextBlock(previosBlockTimeStamp, previosBlockTimeStampMs);
    }
}


void Schain::blockCommitArrived(block_id _committedBlockID, schain_index _proposerIndex, uint64_t _committedTimeStamp,
                                uint64_t _committedTimeStampMs, ptr<ThresholdSignature> _thresholdSig) {


    MONITOR2(__CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime())

    checkForExit();

    ASSERT(_committedTimeStamp < (uint64_t) 2 * MODERN_TIME);

    LOCK(m)


    if (_committedBlockID <= getLastCommittedBlockID())
        return;

    ASSERT(_committedBlockID == (getLastCommittedBlockID() + 1) || getLastCommittedBlockID() == 0);

    try {

        ptr<BlockProposal> committedProposal = nullptr;

        lastCommittedBlockTimeStamp = _committedTimeStamp;
        lastCommittedBlockTimeStampMs = _committedTimeStampMs;


        committedProposal = blockProposalsDatabase->getBlockProposal(_committedBlockID, _proposerIndex);
        ASSERT(committedProposal);

        auto newCommittedBlock = CommittedBlock::makeObject(committedProposal, _thresholdSig);

        processCommittedBlock(newCommittedBlock);

        proposeNextBlock(_committedTimeStamp, _committedTimeStampMs);

    } catch (ExitRequestedException &e) { throw; }
    catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}


void Schain::checkForExit() {
    if (getNode()->isExitRequested()) {
        BOOST_THROW_EXCEPTION(ExitRequestedException(__CLASS_NAME__));
    }
}

void Schain::proposeNextBlock(uint64_t _previousBlockTimeStamp, uint32_t _previousBlockTimeStampMs) {


    MONITOR2(__CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime())

    checkForExit();
    try {

        block_id _proposedBlockID((uint64_t) lastCommittedBlockID + 1);

        if (getNode()->getProposalHashDb()->haveProposal(_proposedBlockID, getSchainIndex()))
            return;


        auto myProposal = pendingTransactionsAgent->buildBlockProposal(_proposedBlockID, _previousBlockTimeStamp,
                                                                       _previousBlockTimeStampMs);

        CHECK_STATE(myProposal->getProposerIndex() == getSchainIndex());
        CHECK_STATE(myProposal->getSignature() != nullptr);


        proposedBlockArrived(myProposal);

        LOG(debug, "PROPOSING BLOCK NUMBER:" + to_string(_proposedBlockID));

        auto db = getNode()->getProposalHashDb();

        db->checkAndSaveHash(_proposedBlockID, getSchainIndex(),
                             myProposal->getHash()->toHex());

        blockProposalClient->enqueueItem(myProposal);
        auto mySig = getSchain()->getCryptoManager()->signDAProofSigShare(myProposal);
        getSchain()->daProofSigShareArrived(mySig, myProposal);

    } catch (ExitRequestedException &e) { throw; }
    catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}

void Schain::processCommittedBlock(ptr<CommittedBlock> _block) {

    CHECK_STATE(_block->getSignature() != nullptr);
    MONITOR2(__CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime())

    checkForExit();


    LOCK(m)

    try {

        ASSERT(getLastCommittedBlockID() + 1 == _block->getBlockID());

        totalTransactions += _block->getTransactionList()->size();

        auto h = _block->getHash()->toHex()->substr(0, 8);
        LOG(info,
            "BLOCK_COMMIT: PRPSR:" + to_string(_block->getProposerIndex()) + ":BID: " +
            to_string(_block->getBlockID()) +
            ":HASH:" + h + ":BLOCK_TXS:" + to_string(_block->getTransactionCount()) + ":DMSG:" +
            to_string(getMessagesCount()) + ":MPRPS:" + to_string(MyBlockProposal::getTotalObjects()) + ":RPRPS:" +
            to_string(ReceivedBlockProposal::getTotalObjects()) + ":TXS:" + to_string(Transaction::getTotalObjects()) +
            ":TXLS:" + to_string(TransactionList::getTotalObjects()) +
            ":KNWN:" + to_string(pendingTransactionsAgent->getKnownTransactionsSize()) + ":MGS:" +
            to_string(Message::getTotalObjects()) + ":INSTS:" + to_string(ProtocolInstance::getTotalObjects()) +
            ":BPS:" +
            to_string(BlockProposalSet::getTotalObjects()) + ":TLS:" + to_string(TransactionList::getTotalObjects()) +
            ":HDRS:" + to_string(Header::getTotalObjects()) + ":SOCK:" + to_string(ClientSocket::getTotalSockets()) +
            ":CONS:" + to_string(ServerConnection::getTotalConnections()));


        saveBlock(_block);

        blockProposalsDatabase->cleanOldBlockProposals(_block->getBlockID());

        pushBlockToExtFace(_block);

        lastCommittedBlockID++;

    } catch (ExitRequestedException &e) { throw; }
    catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}

void Schain::saveBlock(ptr<CommittedBlock> &_block) {
    MONITOR(__CLASS_NAME__, __FUNCTION__)

    try {

        checkForExit();
        getNode()->getBlockDB()->saveBlock(_block);
    } catch (ExitRequestedException &) { throw; } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}


void Schain::pushBlockToExtFace(ptr<CommittedBlock> &_block) {

    MONITOR2(__CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime())

    checkForExit();

    try {

        auto tv = _block->getTransactionList()->createTransactionVector();

        //auto next_price = // VERIFY PRICING

        this->pricingAgent->calculatePrice(*tv, _block->getTimeStamp(), _block->getTimeStampMs(), _block->getBlockID());

        auto cur_price = this->pricingAgent->readPrice(_block->getBlockID() - 1);


        if (extFace) {
            extFace->createBlock(*tv, _block->getTimeStamp(), _block->getTimeStampMs(),
                                 (__uint64_t) _block->getBlockID(),
                                 cur_price);
        }

    } catch (ExitRequestedException &e) { throw; }
    catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}


void Schain::startConsensus(const block_id _blockID) {
    {

        MONITOR(__CLASS_NAME__, __FUNCTION__)

        checkForExit();

        LOG(debug, "Got proposed block set for block:" + to_string(_blockID));

        ASSERT(blockProposalsDatabase->isTwoThird(_blockID));

        LOG(debug, "StartConsensusIfNeeded BLOCK NUMBER:" + to_string((_blockID)));

        LOCK(m)

        if (_blockID <= getLastCommittedBlockID()) {
            LOG(debug, "Too late to start consensus: already committed " + to_string(lastCommittedBlockID));
            return;
        }

        if (_blockID > getLastCommittedBlockID() + 1) {
            LOG(debug, "Consensus is in the future" + to_string(lastCommittedBlockID));
            return;
        }

        if (startedConsensuses.count(_blockID) > 0) {
            LOG(debug, "already started consensus for this block id");
            return;
        }

        startedConsensuses.insert(_blockID);
    }


    auto proposalVector = blockProposalsDatabase->getBooleanProposalsVector(_blockID);

    ASSERT(blockConsensusInstance != nullptr && proposalVector != nullptr);


    auto message = make_shared<ConsensusProposalMessage>(*this, _blockID, proposalVector);

    auto envelope = make_shared<InternalMessageEnvelope>(ORIGIN_EXTERNAL, message, *this);


    LOG(debug, "Starting consensus for block id:" + to_string(_blockID));

    postMessage(envelope);
}

void Schain::daProofArrived(ptr<DAProof> _proof) {

    MONITOR(__CLASS_NAME__, __FUNCTION__)

    try {

        if (_proof->getBlockId() <= getLastCommittedBlockID())
            return;


        if (blockProposalsDatabase->addDAProof(_proof)) {
            startConsensus(_proof->getBlockId());
        }
    } catch (ExitRequestedException &e) { throw; }
    catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}


void Schain::proposedBlockArrived(ptr<BlockProposal> pbm) {

    MONITOR(__CLASS_NAME__, __FUNCTION__)

    CHECK_STATE(pbm->getSignature() != nullptr);


    if (pbm->getBlockID() <= getLastCommittedBlockID())
        return;

    if (blockProposalsDatabase->addBlockProposal(pbm)) {
        //startConsensus(pbm->getBlockID());
    }
}


void Schain::bootstrap(block_id _lastCommittedBlockID, uint64_t _lastCommittedBlockTimeStamp) {

    checkForExit();

    MONITOR2(__CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime())

    try {
        ASSERT(bootStrapped == false);
        bootStrapped = true;
        bootstrapBlockID.store((uint64_t) _lastCommittedBlockID);
        ASSERT(_lastCommittedBlockTimeStamp < (uint64_t) 2 * MODERN_TIME);

        LOCK(m)

        ptr<BlockProposal> committedProposal = nullptr;

        lastCommittedBlockID = (uint64_t) _lastCommittedBlockID;
        lastCommittedBlockTimeStamp = _lastCommittedBlockTimeStamp;
        lastCommittedBlockTimeStampMs = 0;


        LOG(info, "Jump starting the system with block:" + to_string(_lastCommittedBlockID));
        if (getLastCommittedBlockID() == 0)
            this->pricingAgent->calculatePrice(ConsensusExtFace::transactions_vector(), 0, 0, 0);

        proposeNextBlock(lastCommittedBlockTimeStamp, lastCommittedBlockTimeStampMs);

    } catch (Exception &e) {
        Exception::logNested(e);
        return;
    }
}


void Schain::healthCheck() {


    std::unordered_set<uint64_t> connections;

    setHealthCheckFile(1);

    auto beginTime = Time::getCurrentTimeSec();

    LOG(info, "Waiting to connect to peers");

    while (3 * (connections.size() + 1) < 2 * getNodeCount()) {
        if (Time::getCurrentTimeSec() - beginTime > 6000) {
            setHealthCheckFile(0);
            LOG(err, "Coult not connect to 2/3 of peers");
            exit(110);
        }

        for (int i = 1; i <= getNodeCount(); i++) {
            if (i != (getSchainIndex()) && !connections.count(i)) {
                try {
                    if (getNode()->isExitRequested()) {
                        BOOST_THROW_EXCEPTION(ExitRequestedException(__CLASS_NAME__));
                    }
                    auto socket = make_shared<ClientSocket>(*this, schain_index(i), port_type::PROPOSAL);
                    LOG(debug, "Health check: connected to peer");
                    getIo()->writeMagic(socket, true);
                    connections.insert(i);
                } catch (ExitRequestedException &) {
                    throw;
                } catch (std::exception &e) {
                    usleep(1000000);
                }
            }
        }
    }

    setHealthCheckFile(2);
}

void Schain::daProofSigShareArrived(ptr<ThresholdSigShare> _sigShare, ptr<BlockProposal> _proposal) {

    MONITOR(__CLASS_NAME__, __FUNCTION__)

    checkForExit();
    CHECK_ARGUMENT(_sigShare != nullptr);
    CHECK_ARGUMENT(_proposal != nullptr);


    try {
        auto proof = receivedDASigSharesDatabase->addAndMergeSigShareAndVerifySig(_sigShare, _proposal);
        if (proof != nullptr) {
            getSchain()->daProofArrived(proof);
            blockProposalClient->enqueueItem(proof);
        }
    } catch (ExitRequestedException &) { throw; } catch (...) {
        LOG(err, "Could not add/merge sig");
        throw_with_nested(InvalidStateException("Could not add/merge sig", __CLASS_NAME__));
    }
}


void Schain::constructServers(ptr<Sockets> _sockets) {

    MONITOR(__CLASS_NAME__, __FUNCTION__)

    blockProposalServerAgent = make_shared<BlockProposalServerAgent>(*this, _sockets->blockProposalSocket);
    catchupServerAgent = make_shared<CatchupServerAgent>(*this, _sockets->catchupSocket);
}

ptr<BlockProposal> Schain::createEmptyBlockProposal(block_id _blockId) {

    uint64_t sec = lastCommittedBlockTimeStamp;
    uint64_t ms = lastCommittedBlockTimeStampMs;

// Set time for an empty block to be 1 ms more than previous block
    if (ms == 999) {
        sec++;
        ms = 0;
    } else {
        ms++;
    }


    return make_shared<ReceivedBlockProposal>(*this, _blockId, sec, ms);

}


void Schain::decideBlock(block_id _blockId, schain_index _proposerIndex, ptr<ThresholdSignature> _thresholdSig) {

    CHECK_ARGUMENT(_thresholdSig != nullptr);

    MONITOR2(__CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime())


    LOG(debug, "decideBlock:" + to_string(_blockId) + ":PRP:" + to_string(_proposerIndex));
    LOG(debug, "Total txs:" + to_string(getSchain()->getTotalTransactions()) + " T(s):" +
               to_string((Time::getCurrentTimeMs() - getSchain()->getStartTimeMs()) / 1000.0));


    auto proposedBlockSet = blockProposalsDatabase->getProposedBlockSet(_blockId);

    ptr<BlockProposal> proposal = nullptr;
    ASSERT(proposedBlockSet);

    if (_proposerIndex == 0) {
        proposal = createEmptyBlockProposal(_blockId);
    } else {
        proposal = proposedBlockSet->getProposalByIndex(_proposerIndex);
    }


    if (proposal == nullptr ||
        proposal->getDaProof() == nullptr ||
        // this switch is for testing only
        getNode()->getTestConfig()->isFinalizationDownloadOnly()) {

        // did not receive proposal from the proposer, pull it in parallel from other hosts
        // Note that due to the BLS signature proof, 2t hosts out of 3t + 1 total are guaranteed to
        // posess the proposal

        auto agent = make_unique<BlockFinalizeDownloader>(this, _blockId, _proposerIndex, proposedBlockSet);

        // This will complete successfully also if block arrives through catchup
        proposal = agent->downloadProposal();

        if (proposal != nullptr) // Nullptr means catchup happened first
            proposedBlockSet->add(proposal);
    }

    if (proposal != nullptr)
        blockCommitArrived(_blockId, _proposerIndex, proposal->getTimeStamp(), proposal->getTimeStampMs(),
                           _thresholdSig);

}
