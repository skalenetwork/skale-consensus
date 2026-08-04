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

#include <algorithm>
#include "leveldb/db.h"
#include <malloc.h>
#include <sched.h>
#include <unordered_set>

#ifdef BITE
#include <folly/executors/CPUThreadPoolExecutor.h>
#endif

#include "Log.h"
#include "SkaleCommon.h"
#include "exceptions/FatalError.h"
#include "exceptions/InvalidArgumentException.h"
#include "thirdparty/json.hpp"


#include "abstracttcpserver/ConnectionStatus.h"
#include "blockproposal/pusher/BlockProposalClientAgent.h"
#include "db/MsgDB.h"
#include "exceptions/InvalidStateException.h"
#include "headers/BlockProposalRequestHeader.h"
#include "network/Network.h"
#include "network/Utils.h"
#include "node/ConsensusEngine.h"
#include "node/Node.h"
#include "pendingqueue/PendingTransactionsAgent.h"
#include "utils/Time.h"

#include "blockfinalize/client/BlockFinalizeDownloader.h"
#include "blockproposal/server/BlockProposalServerAgent.h"
#include "catchup/client/CatchupClientAgent.h"
#include "catchup/server/CatchupServerAgent.h"
#include "crypto/BLAKE3Hash.h"
#include "crypto/ConsensusBLSSigShare.h"
#include "crypto/ThresholdSignature.h"
#include "datastructures/BlockProposal.h"
#include "datastructures/BlockProposalSet.h"
#include "datastructures/BooleanProposalVector.h"
#include "datastructures/CommittedBlock.h"
#include "datastructures/CommittedBlockList.h"
#include "datastructures/DAProof.h"
#include "datastructures/MyBlockProposal.h"
#include "datastructures/ReceivedBlockProposal.h"
#include "datastructures/TimeStamp.h"
#include "datastructures/Transaction.h"
#include "datastructures/TransactionList.h"
#include "db/BlockProposalDB.h"
#include "db/BlockSigShareDB.h"
#include "db/DAProofDB.h"
#include "db/DASigShareDB.h"
#include "db/InternalInfoDB.h"
#include "db/PriceDB.h"
#include "db/ProposalVectorDB.h"
#include "db/RandomDB.h"
#include "exceptions/EngineInitException.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/FatalError.h"
#include "exceptions/ParsingException.h"
#include "messages/ConsensusProposalMessage.h"
#include "messages/InternalMessageEnvelope.h"
#include "messages/Message.h"
#include "messages/MessageEnvelope.h"
#include "messages/NetworkMessageEnvelope.h"
#include "monitoring/MonitoringAgent.h"
#include "monitoring/StuckDetectionAgent.h"
#include "monitoring/OptimizerAgent.h"
#include "network/ClientSocket.h"
#include "network/IO.h"
#include "network/Sockets.h"
#include "network/ZMQSockets.h"
#include "node/NodeInfo.h"
#ifndef FAIR
#include "oracle/OracleClient.h"
#include "oracle/OracleMessageThreadPool.h"
#include "oracle/OracleResultAssemblyAgent.h"
#include "oracle/OracleServerAgent.h"
#include "oracle/OracleThreadPool.h"
#endif
#include "pricing/PricingAgent.h"
#include "protocols/ProtocolInstance.h"
#include "protocols/blockconsensus/BlockConsensusAgent.h"


#include "Schain.h"

#include <statusserver/StatusServer.h>

#include "SchainMessageThreadPool.h"
#include "SchainTest.h"
#include "TestConfig.h"
#include "crypto/CryptoManager.h"
#include "crypto/ThresholdSigShare.h"
#include "chains/BlockErrorAnalyzer.h"

#ifdef BITE
#include "bite/BiteManager.h"
#include "crypto/DecryptedAESKeyList.h"
#include "db/TEDecryptionDB.h"

#include "protocols/blockconsensus/ConsensusSignatureDomains.h"
#endif // BITE

#include "db/BlockDB.h"
#include "db/CacheLevelDB.h"
#include "db/ProposalHashDB.h"

#include "libBLS/bls/BLSPrivateKeyShare.h"
#include "monitoring/LivelinessMonitor.h"
#include "monitoring/TimeoutAgent.h"
#include "pendingqueue/TestMessageGeneratorAgent.h"

template<class M>
class try_lock_timed_guard {
private:
    M &mtx_;
    std::atomic_bool was_locked_;

    bool try_lock(const size_t nNumberOfMilliseconds) {
        auto now = std::chrono::steady_clock::now();
        if (mtx_.try_lock_until(now + std::chrono::milliseconds(nNumberOfMilliseconds)))
            return true; // was locked
        return false;
    }

public:
    explicit try_lock_timed_guard(M &mtx, const size_t nNumberOfMilliseconds = 1000)
        : mtx_(mtx), was_locked_(false) {
        was_locked_ = try_lock(nNumberOfMilliseconds);
    }

    ~try_lock_timed_guard() {
        if (was_locked_)
            mtx_.unlock();
    }

    bool was_locked() const { return was_locked_; }
};

void Schain::postMessage(const ptr<MessageEnvelope> &_me) {
    CHECK_ARGUMENT(_me);

    MONITOR(__CLASS_NAME__, __FUNCTION__)

    checkForExit();

    CHECK_STATE(( uint64_t ) _me->getMessage()->getBlockId() != 0); {
        lock_guard<mutex> l(messageMutex);
        messageQueue.push(_me);
        messageCond.notify_all();
    }
}


void Schain::messageThreadProcessingLoop(Schain *_sChain) {
    CHECK_ARGUMENT(_sChain);
    logThreadLocal_ = _sChain->getNode()->getLog();

    setThreadName("msgThreadProcLoop", _sChain->getNode()->getConsensusEngine());

    _sChain->waitOnGlobalStartBarrier();

    try {
        _sChain->startTimeMs = Time::getCurrentTimeMs();

        queue<ptr<MessageEnvelope> > newQueue;

        while (!_sChain->getNode()->isExitRequested()) {
            {
                unique_lock<mutex> mlock(_sChain->messageMutex);
                while (_sChain->messageQueue.empty()) {
                    _sChain->messageCond.wait(mlock);
                    if (_sChain->getNode()->isExitRequested())
                        return;
                }

                newQueue = _sChain->messageQueue;

                while (!_sChain->messageQueue.empty()) {
                    if (_sChain->getNode()->isExitRequested())
                        return;

                    _sChain->messageQueue.pop();
                }
            }

            while (!newQueue.empty()) {
                if (_sChain->getNode()->isExitRequested())
                    return;

                ptr<MessageEnvelope> m = newQueue.front();
                CHECK_STATE(( uint64_t ) m->getMessage()->getBlockId() != 0);

                try {
                    _sChain->getBlockConsensusInstance()->routeAndProcessMessage(m);
                } catch (exception &e) {
                    CONS_LOG(err, "Exception in Schain::messageThreadProcessingLoop");
                    SkaleException::logNested(e);
                    if (_sChain->getNode()->isExitRequested())
                        return;
                } // catch

                newQueue.pop();
            }
        }
    } catch (FatalError &e) {
        SkaleException::logNested(e);
        _sChain->getNode()->initiateApplicationExitOnFatalConsensusError(e.what());
    }
}


void Schain::startThreads() {
    if (getNode()->isSyncOnlyNode()) {
        return;
    }
    CHECK_STATE(consensusMessageThreadPool)
    this->consensusMessageThreadPool->startService();
}

const string &Schain::getSchainName() const {
    return schainName;
}

Schain::Schain(weak_ptr<Node> _node, schain_index _schainIndex, const schain_id &_schainID,
               ConsensusExtFace *_extFace, string &_schainName)
    : Agent(*this, true, true),
      totalTransactions(0),
      extFace(_extFace),
      schainID(_schainID),
      schainName(_schainName),
      startTimeMs(0),
      consensusMessageThreadPool(new SchainMessageThreadPool(this)),
      node(_node),
      schainIndex(_schainIndex) {
    lastCommittedBlockTimeStamp = TimeStamp(0, 0);
    setTimeStampValuesFromConfig();

    // construct monitoring, timeout and stuck detection agents early
    monitoringAgent = make_shared<MonitoringAgent>(*this);
    if (!getNode()->isSyncOnlyNode()) {
        timeoutAgent = make_shared<TimeoutAgent>(*this);
        stuckDetectionAgent = make_shared<StuckDetectionAgent>(*this);
    }

    maxExternalBlockProcessingTime =
            std::max(2 * getNode()->getEmptyBlockIntervalMs(), (uint64_t) 3000);

    MONITOR(__CLASS_NAME__, __FUNCTION__)

    if (!getNode()->isSyncOnlyNode()) {
        CHECK_STATE(schainIndex > 0);
        CHECK_STATE(getNode()->getNodeInfosByIndex()->size() > 0);
    }

    try {
        this->io = make_shared<IO>(this);


        for (auto const &iterator: *getNode()->getNodeInfosByIndex()) {
            if (iterator.second->getNodeID() == getNode()->getNodeID()) {
                CHECK_STATE(thisNodeInfo == nullptr && iterator.second != nullptr);
                thisNodeInfo = iterator.second;
            }
        }

        if (thisNodeInfo == nullptr && !getNode()->isSyncOnlyNode()) {
            BOOST_THROW_EXCEPTION(EngineInitException(
                "Schain: " + to_string( ( uint64_t ) getSchainID() ) +
                " does not include current node with IP " + getNode()->getBindIP() +
                "and node id " + to_string( getNode()->getNodeID() ),
                __CLASS_NAME__ ));
        }

        CHECK_STATE(getNodeCount() > 0);

        constructChildAgents();

        startStatusServer();

        string none = SchainTest::NONE;

        blockProposerTest = none;

        getNode()->registerAgent(this);
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        throw_with_nested(FatalError(__FUNCTION__, __CLASS_NAME__));
    }
}

Schain::~Schain() {

    auto n = node.lock();
    if ( n ) {
        // Signal all agent threads to exit their loops
        n->exitRequested = true;

        // Release barriers so threads blocked on waitOnGlobalStartBarrier() can wake up.
        // This is a no-op if barriers were already released by startServers().
        n->releaseGlobalServerBarrier();
        n->releaseGlobalClientBarrier();
    }

    // MonitoringAgent requires explicit stop() because it uses a condition_variable
    // for sleeping, not the global start barrier. The stop() signals the condition
    // variable to wake up immediately rather than waiting for the sleep interval.
    if ( monitoringAgent ) {
        monitoringAgent->stop();
        monitoringAgent->join();
    }

    // TimeoutAgent and StuckDetectionAgent will exit their loops once they detect
    // exitRequested is true (checked after barrier wait and in their main loops).
    if ( timeoutAgent ) {
        timeoutAgent->join();
    }
    if ( stuckDetectionAgent ) {
        stuckDetectionAgent->join();
    }
}

// called from constructor so no locks needed
void Schain::constructChildAgents() {
    MONITOR(__CLASS_NAME__, __FUNCTION__)

    try {
        optimizerAgent = make_shared<OptimizerAgent>(*this);
#ifndef FAIR
        oracleResultAssemblyAgent = make_shared< OracleResultAssemblyAgent >( *this );
#endif
        pricingAgent = make_shared<PricingAgent>(*this);
        catchupClientAgent = make_shared<CatchupClientAgent>(*this);

        cryptoManager = make_shared<CryptoManager>(*this);

#ifdef BITE
        biteManager = make_shared<BiteManager>(*this);
        finalizationExecutor = ::make_shared<folly::CPUThreadPoolExecutor>(1);
#endif


        if (getNode()->isSyncOnlyNode()) {
            return;
        }




        pendingTransactionsAgent = make_shared<PendingTransactionsAgent>(*this);
        blockProposalClient = make_shared<BlockProposalClientAgent>(*this);

        testMessageGeneratorAgent = make_shared<TestMessageGeneratorAgent>(*this);
#ifndef FAIR
        oracleClient = make_shared< OracleClient >( *this );
#endif
    } catch (...) {
        throw_with_nested(FatalError(__FUNCTION__, __CLASS_NAME__));
    }
}


void Schain::lockWithDeadLockCheck(const char *_functionName) {
    while (!blockProcessMutex.try_lock_for(chrono::seconds(60))) {
        CONS_LOG(err, "Trying to lock in:" << string( _functionName ));
    }
}


[[nodiscard]] uint64_t Schain::blockCommitsArrivedThroughCatchup(
    const ptr<CommittedBlockList> &_blockList, uint64_t _catchupDownloadTimeMs) {
    CHECK_ARGUMENT(_blockList);

    auto blocks = _blockList->getBlocks();

    CHECK_STATE(blocks);

    if (blocks->size() == 0) {
        return 0;
    }


    // wait until the schain state is fully initialized and startup
    // otherwise last committed block id is not fully initialized and the chain can not accept
    // catchup blocks
    while (!getSchain()->getIsStateInitialized()) {
        usleep(500 * 1000);
        CONS_LOG(info, "Waiting for boostrap to complete ...");
    }

    // the node could fail to proccess a block in usual way
    // but then it downloads missing blocks through catchup
    if ( proposalStageStartTimeMs > 0 )
        proposalStageFinishTimeMs = Time::getCurrentTimeMs();
    if ( blockFinalizationStartTimeMs > 0 )
        blockFinalizationFinishTimeMs = Time::getCurrentTimeMs();

    auto catchupProcessStartTimeMs = Time::getCurrentTimeMs();

    try {
        if (!blockProcessMutex.try_lock_for(chrono::seconds(60))) {
            // Could not lock for 60 seconds. There is probably a deadlock.
            // Skipping this catchup iteration
            checkForExit();
            CONS_LOG(err, "Could not lock in:" << string( __FUNCTION__ ));
            return 0;
        }

        auto catchupLockTimeMs = Time::getCurrentTimeMs() - catchupProcessStartTimeMs;

        bumpPriority();

        atomic<uint64_t> committedIDOld = (uint64_t) getLastCommittedBlockID();

        CHECK_STATE(blocks->at( 0 )->getBlockID() <= ( uint64_t ) getLastCommittedBlockID() + 1);

        for (size_t i = 0; i < blocks->size(); i++) {
            checkForExit();

            auto block = blocks->at(i);

            CHECK_STATE(block);

            if ((uint64_t) block->getBlockID() == (getLastCommittedBlockID() + 1)) {
                CHECK_STATE(getLastCommittedBlockTimeStamp() < block->getTimeStamp());
                processCommittedBlock(block);
            }
        }

        uint64_t result = 0;

        auto catchupProcessTimeMs = Time::getCurrentTimeMs() - catchupProcessStartTimeMs;

        if (committedIDOld < getLastCommittedBlockID()) {
            CONS_LOG(info, "CATCHUP_PROCESSED_BLOCKS:COUNT:"
                << to_string( getLastCommittedBlockID() - committedIDOld )
                << ":DTM:" << _catchupDownloadTimeMs << ":PTM:" << catchupProcessTimeMs
                << ":LTM:" << catchupLockTimeMs);
            result = ((uint64_t) getLastCommittedBlockID()) - committedIDOld;
            if (!getNode()->isSyncOnlyNode()) {
                proposeNextBlock(true);
            } else {
                // on sync nodes we get candidate block and throw it away immediately
                // this is to clean skaled queues
                if (extFace) {
                    // if extFace is null we are in consensus tests and there is no
                    // skaled
                    u256 stateRoot = 0;
                    extFace->pendingTransactions(
                        getNode()->getMaxTransactionsPerBlock(), stateRoot);
                }
            }
        }

        unbumpPriority();

        // we need to unlock the mutex everytime we return from the function
        // including exceptions. In 2.3 we will make it cleaner by using
        // a custom lock_guard so the lock is unlocked automatically
        blockProcessMutex.unlock();
        return result;
    } catch (...) {
        unbumpPriority();
        blockProcessMutex.unlock();
        throw;
    }
}

const atomic<bool> &Schain::getIsStateInitialized() const {
    return isStateInitialized;
}

uint64_t Schain::getVerifyDaSigsPatchTimeStamp() const {
    return verifyDaSigsPatchTimestamp;
}

bool Schain::verifyDASigsPatch(uint64_t
#ifndef BITE
        _blockTimeStampS
#endif
) {
#ifdef BITE
    return true;
#else
    return verifyDaSigsPatchTimestamp != 0 && _blockTimeStampS >= verifyDaSigsPatchTimestamp;
#endif
}

uint64_t Schain::getVerifyBlsSyncPatchTimestampS() const {
    return verifyBlsSyncPatchTimestamp;
}

bool Schain::verifyBlsSyncPatch(uint64_t
#ifndef BITE
        _blockTimeStampS
#endif
) {
#ifdef BITE
    return true;
#else
    return verifyBlsSyncPatchTimestamp != 0 && _blockTimeStampS >= verifyBlsSyncPatchTimestamp;
#endif
}

#ifdef BITE
bool Schain::bite2Patch(uint64_t _blockTimeStampSec) {
    return bite2PatchTimestamp != 0 && _blockTimeStampSec >= bite2PatchTimestamp;
}

uint64_t Schain::getBITE2PatchTimestampS() const {
    return bite2PatchTimestamp;
}
#endif

void Schain::blockCommitArrived(block_id _committedBlockID, schain_index _proposerIndex,
                                const ptr<ThresholdSignature> &_thresholdSig, 
#ifdef BITE
                                const ptr<ThresholdSignature> &_reencryptionThresholdSig,
#endif
                                ptr<ThresholdSignature> _daSig
#ifdef  BITE
    , ptr< DecryptedAESKeyList > _aesKeyList, DecryptedTransactions _decryptedTransactions
#endif
) {
    MONITOR2(__CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime())

    CHECK_ARGUMENT(_thresholdSig)
    CHECK_ARGUMENT(_daSig || _proposerIndex == 0)

#ifdef BITE
    CHECK_ARGUMENT(_aesKeyList)
    bool isBite2PatchEnabled = bite2Patch( getLastCommittedBlockTimeStamp().getS() );
    if ( isBite2PatchEnabled ) {
        CHECK_ARGUMENT(_reencryptionThresholdSig)
    }
    else {
        // enforce bite2 blocks are rejected if we are not yet in bite2 patch
        CHECK_ARGUMENT(!_reencryptionThresholdSig)
    }
#endif

    // wait until the schain state is fully initialized and startup
    // otherwise last committed block id is not fully initialized and the chain can not accept
    // catchup blocks
    while (!getSchain()->getIsStateInitialized()) {
        usleep(500 * 1000);
        CONS_LOG(info, "Waiting for boostrap to complete ...");
    }

    // no regular block commits happen for sync nodes
    CHECK_STATE(!getNode()->isSyncOnlyNode());

    checkForExit();

    try {
        lockWithDeadLockCheck(__FUNCTION__);


        if (_committedBlockID <= getLastCommittedBlockID()) {
            // we meed to unlock the mutex everytime we return from the function
            // including exceptions. In 2.3 we will make it cleaner by using
            // a custom lock_guard so the lock is unlocked automatically
            // for now we just manually verify in the code that the mutex is always unlocked
            blockProcessMutex.unlock();
            return;
        }


        CHECK_STATE(_committedBlockID == ( getLastCommittedBlockID() + 1 ) ||
            getLastCommittedBlockID() == 0);

        bumpPriority();


        ptr<BlockProposal> committedProposal = nullptr;

        if (_proposerIndex > 0) {
            committedProposal = getNode()->getBlockProposalDB()->getBlockProposal(
                _committedBlockID, _proposerIndex);
        } else {
            committedProposal = createDefaultEmptyBlockProposal(_committedBlockID
#ifdef BITE
             , getNode()->getCurrentEpochId()
#endif
            );
        }

        CHECK_STATE(committedProposal);

        auto newCommittedBlock =
                CommittedBlock::makeFromProposal(committedProposal, _thresholdSig, 
#ifdef BITE
                    _reencryptionThresholdSig,
#endif
                    _daSig
#ifdef BITE
            , _aesKeyList, _decryptedTransactions
#endif
                );

        CHECK_STATE(getLastCommittedBlockTimeStamp() < newCommittedBlock->getTimeStamp());


        processCommittedBlock(newCommittedBlock);
        proposeNextBlock(false);

        unbumpPriority();

        blockProcessMutex.unlock();
    } catch (ExitRequestedException &e) {
        unbumpPriority();
        blockProcessMutex.unlock();
        throw;
    } catch (...) {
        unbumpPriority();
        blockProcessMutex.unlock();
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


void Schain::checkForExit() {
    if (getNode()->isExitRequested()) {
        BOOST_THROW_EXCEPTION(ExitRequestedException( __CLASS_NAME__ ));
    }
}


// Note: this function must be called with blockProcessing mutex held
void Schain::proposeNextBlock(bool _isCalledAfterCatchup) {
    MONITOR2(__CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime())

    checkForExit();
    try {
        block_id _proposedBlockID((uint64_t) lastCommittedBlockID + 1);

        ptr<BlockProposal> myProposal;
#ifdef BITE
        bool isProposalCameFromDb = false;
#endif        
        proposalStageStartTimeMs = Time::getCurrentTimeMs();
        if ( getNode()->getProposalHashDB()->haveProposal( _proposedBlockID, getSchainIndex() ) ) {
#ifdef BITE            
            isProposalCameFromDb = true;
#endif
            myProposal = getNode()->getBlockProposalDB()->getBlockProposal(
                _proposedBlockID, getSchainIndex());
            // getBlockProposal() already calls for verifyAndCreateMyDecryptionSharesForProposalTransactions
            // no need to call it twice
        } else {
            auto stamp = getLastCommittedBlockTimeStamp();
            myProposal = pendingTransactionsAgent->buildBlockProposal(
                _proposedBlockID, stamp, _isCalledAfterCatchup);
        }

        CHECK_STATE(myProposal);

        CHECK_STATE(myProposal->getProposerIndex() == getSchainIndex());
        CHECK_STATE(myProposal->getSignature() != "");

#ifdef BITE
        // Parse and validate BITE ciphertexts
        if (!isProposalCameFromDb) {
            getSchain()->getBiteManager()->computeAndValidateSGXAESKeyBatch(myProposal);

            if (!myProposal->getFailedTransactionsRef().empty()) {
                CONS_LOG(err, "Critical error - invalid BITE transactions");
                CONS_LOG(err, "Rejecting proposal and triggering fallback consensus for default block");
                try {
                    timeoutAgent->forceEarlyTimeout();
                } catch (ExitRequestedException &) {
                    throw;
                } catch (...) {
                    CONS_LOG(err, "Could not trigger fallback consensus for default block");
                }
                return;
            }
        }
#endif

        // only after the proposal is fully built and validated we can announce it to the network
        proposedBlockArrived(myProposal);

        if (getOptimizerAgent()->skipSendingProposalToTheNetwork(_proposedBlockID)) {
            // a node skips sending and saving its proposal during
            // optimized block consensus, if the node was not a winner
            // last time
            return; // dont propose
        }

#ifdef BITE
        if (!isProposalCameFromDb) {
            // SGX decryption shares will be computed asynchronously
            getBiteManager()->scheduleSGXToCreateMyDecryptionSharesForProposalTransactions(
                myProposal);
        }
#endif

        CONS_LOG(debug, "PROPOSING BLOCK NUMBER:" << to_string( _proposedBlockID ));

        auto db = getNode()->getProposalHashDB();

        db->checkAndSaveHash(_proposedBlockID, getSchainIndex(), myProposal->getHash().toHex());

        blockProposalClient->enqueueItem(myProposal);

        auto [mySig, ecdsaSig, pubKey, pubKeySig] =
                getSchain()->getCryptoManager()->signDAProof(myProposal);

        CHECK_STATE(mySig);

        // make compiler happy
        ecdsaSig = "";
        pubKey = "";
        pubKeySig = "";

        getSchain()->daProofSigShareArrived(mySig, myProposal);
    
    } catch (ExitRequestedException &e) {
        throw;
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}

void Schain::bumpPriority() {
    // temporary bump thread priority
    // We'll operate on the currently running thread.
    pthread_t this_thread = pthread_self();
    struct sched_param params;
    // We'll set the priority to the maximum.
    params.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(this_thread, SCHED_FIFO, &params);
}

void Schain::unbumpPriority() {
    struct sched_param params;
    // Set the priority to norm
    pthread_t this_thread = pthread_self();
    params.sched_priority = 0;
    CHECK_STATE(pthread_setschedparam( this_thread, 0, &params ) == 0)
}


void Schain::saveToVisualization(ptr<CommittedBlock> _block, uint64_t _visualizationType) {
    CHECK_STATE(_block);


    string info = string("{") + "\"t\":" + to_string(MsgType::MSG_BLOCK_COMMIT) + "," +
                  "\"b\":" + to_string(Time::getCurrentTimeMs() - getStartTimeMs()) + "," +
                  "\"s\":" + to_string(getSchain()->getSchainIndex()) + "," +
                  "\"p\":" + to_string(_block->getProposerIndex()) + "," +
                  "\"i\":" + to_string(_block->getBlockID()) + "}\n";

    if (_visualizationType == 1)
        Schain::writeToVisualizationStream(info);
}

void Schain::printBlockLog(const ptr<CommittedBlock> &_block) {
    CHECK_STATE(_block);

    MONITOR2(__CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime())

    totalTransactions += _block->getTransactionList()->size();

    auto h = _block->getHash().toHex().substr(0, 8);

    auto stamp = TimeStamp(_block->getTimeStampS(), _block->getTimeStampMs());


    stringstream output;

    output << "BLOCK_COMMITED: PRPSR:" << _block->getProposerIndex()
            << ":BID: " << _block->getBlockID()
            << ":ROOT:" << _block->getStateRoot().convert_to<string>() << ":HASH:" << h
            << ":BLOCK_TXS:" << _block->getTransactionCount() << ":DMSG:" << getMessagesCount()
            << ":TPRPS:" << BlockProposal::getTotalObjects()
            << ":MPRPS:" << MyBlockProposal::getTotalObjects()
            << ":RPRPS:" << ReceivedBlockProposal::getTotalObjects()
            << ":TXS:" << Transaction::getTotalObjects()
            << ":TXLS:" << TransactionList::getTotalObjects()
            << ":MGS:" << Message::getTotalObjects()
            << ":INSTS:" << ProtocolInstance::getTotalObjects()
            << ":BPS:" << BlockProposalSet::getTotalObjects()
            << ":HDRS:" << Header::getTotalObjects() << ":SOCK:" << ClientSocket::getTotalSockets()
            << ":FDS:" << ConsensusEngine::getOpenDescriptors() << ":PRT:" << proposalReceiptTime
            << ":BTA:" << blockTimeAverageMs << ":BSA:" << blockSizeAverage << ":TPS:" << tpsAverage
            << ":LWT:" << CacheLevelDB::getWriteStats() << ":LRT:" << CacheLevelDB::getReadStats()
            << ":LWC:" << CacheLevelDB::getWrites() << ":LRC:" << CacheLevelDB::getReads();


    if (!getNode()->isSyncOnlyNode()) {
        output << ":KNWN:" << pendingTransactionsAgent->getKnownTransactionsSize()
               << ":CONS:" << ServerConnection::getTotalObjects()
               << ":DSDS:" << (getSchain()->getNode()->hasNetwork() ? getSchain()->getNode()->getNetwork()->computeTotalDelayedSends() : 0)
               << ":SET:" << CryptoManager::getEcdsaStats()
               << ":SBT:" << CryptoManager::getBLSStats()
#ifdef BITE
               << ":STET:" << CryptoManager::getTEDecryptStats()
#endif
               << ":SEC:" << CryptoManager::getECDSATotals()
               << ":SBC:" << CryptoManager::getBLSTotals()
               << ":ZSC:" << getCryptoManager()->getZMQSocketCount()
               << ":EPT:" << lastCommittedBlockEvmProcessingTimeMs;
    }

    output << ":STAMP:" << stamp.toString();

    CONS_LOG(info, output.str());

    // get periodic stats
    static atomic<uint64_t> counter = 1;

    if (counter % 20 == 0) {
        output.str("");
        output << "LEVELDB_MEM_STATS:BLOCKS:" << getNode()->getBlockDB()->getMemoryUsed();;
        output << ":PROPS:" << getNode()->getBlockProposalDB()->getMemoryUsed();
        output << ":DAPS:" << getNode()->getDaProofDB()->getMemoryUsed();
        output << ":OMS:" << getNode()->getOutgoingMsgDB()->getMemoryUsed();
        output << ":PHS:" << getNode()->getProposalHashDB()->getMemoryUsed();
        output << ":PVS:" << getNode()->getProposalVectorDB()->getMemoryUsed();
        output << ":BSS:" << getNode()->getBlockSigShareDB()->getMemoryUsed();
        output << ":IMS:" << getNode()->getIncomingMsgDB()->getMemoryUsed();
        output << ":RMS:" << getNode()->getRandomDB()->getMemoryUsed();
        output << ":PCS:" << getNode()->getPriceDB()->getMemoryUsed();
        output << ":IIN:" << getNode()->getInternalInfoDB()->getMemoryUsed();
        output << ":DAS:" << getNode()->getDaSigShareDB()->getMemoryUsed();
        CONS_LOG(info, output.str());
        CONS_LOG(info, Utils::getRusage());
    }

    counter++;
}

void Schain::processCommittedBlock(const ptr<CommittedBlock> &_block) {
    CHECK_ARGUMENT(_block);
    // process committed block needs to be called why holding main mutex

    MONITOR2(__CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime())

    checkForExit();

    if (getSchain()->getNode()->getVisualizationType() > 0) {
        saveToVisualization(_block, getSchain()->getNode()->getVisualizationType());
    }

    try {
        CHECK_STATE(getLastCommittedBlockID() + 1 == _block->getBlockID())

        printBlockLog(_block);

        proposalReceiptTime = 0;

        CHECK_STATE(_block->getBlockID() = getLastCommittedBlockID() + 1)


        uint64_t blockCommitStartTimeMs = Time::getCurrentTimeMs();
        saveBlock( _block );
        uint64_t blockCommitTimeMs = Time::getCurrentTimeMs() - blockCommitStartTimeMs;


        cleanupUnneededMemoryBeforePushingToEvm(_block);

        auto evmProcessingStartMs = Time::getCurrentTimeMs();
        auto blockPushedToExtFaceTimeMs = evmProcessingStartMs;

        if (!getNode()->isSyncOnlyNode()) {
            // pending transaction ageent does not exist on a sync node
            CHECK_STATE(pendingTransactionsAgent);
#ifdef BITE
            auto decryptedTxs = _block->getDecryptedTransactions();
            CHECK_STATE(decryptedTxs.regularTxsMap)
            CHECK_STATE(decryptedTxs.ctxTxsMap)
#endif


            CONS_LOG(info, "CWT:" + to_string( blockPushedToExtFaceTimeMs -
                                           pendingTransactionsAgent->transactionListReceivedTime() )
                             + ":TLWT:" + to_string( pendingTransactionsAgent->getTransactionListWaitTime() )
                             + ":SBPT:" + to_string( cryptoManager->sgxBlockProcessingTime() )
#ifdef BITE
                    + ":BITE_DECRYPTED_TXS:" + to_string(decryptedTxs.regularTxsMap->size())
                    + ":CAT_DECRYPTED_TXS:" + to_string(decryptedTxs.ctxTxsMap->size())
#endif

                             );
            CONS_LOG( debug, "BCT:" + std::to_string( blockCommitTimeMs ) +
                        ":BFST:" + std::to_string( getBlockFinalizationStageTimeMs() ) +
                        ":PST:" + std::to_string( getProposalStageTimeMs() ) );
        }

        pushBlockToExtFace(_block);
        auto evmProcessingTimeMs = Time::getCurrentTimeMs() - evmProcessingStartMs;

        auto stamp = TimeStamp(_block->getTimeStampS(), _block->getTimeStampMs());

        updateLastCommittedBlockInfo((uint64_t) _block->getBlockID(), stamp,
                                     _block->getTransactionList()->size(), evmProcessingTimeMs);

        // the last thing is to run analyzers to log any errors that happened during
        // block processing

        analyzeErrors(_block);
    } catch (ExitRequestedException &e) {
        throw;
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}

void Schain::saveBlock(const ptr<CommittedBlock> &_block) {
    CHECK_ARGUMENT(_block);

    MONITOR(__CLASS_NAME__, __FUNCTION__)

    try {
        checkForExit();

#ifdef BITE
        // save random before saving block. If block is ever available in db, random should also be
        // compute reencryption random from block signature & save in random db
        std::optional<string> reencryptionSignature = _block->getReencryptionThresholdSig();
        bool isBite2PatchEnabled = bite2Patch( getLastCommittedBlockTimeStamp().getS() );
        if (isBite2PatchEnabled) {
            CHECK_STATE2(reencryptionSignature.has_value(), 
                "BITE2 patch is enabled but reencryption signature is missing for block " + to_string( _block->getBlockID() ));
            CHECK_STATE2(!reencryptionSignature->empty(), 
                "BITE2 patch is enabled but reencryption signature is empty string for block " + to_string( _block->getBlockID() ));

            auto random = Schain::calculateRandomFromSignatureString( reencryptionSignature.value() );
            getSchain()->getNode()->getRandomDB()->writeDomainRandom(
                blockconsensus::REENCRYPTION_RANDOM_DOMAIN, _block->getBlockID(), random );
        }
        else {
            CHECK_STATE2(!reencryptionSignature.has_value(), 
                "BITE2 patch is not enabled but reencryption signature is present for block " + to_string( _block->getBlockID() ));
        }
#endif

        // save in block db
        getNode()->getBlockDB()->saveBlock(_block);

    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}

void Schain::cleanupUnneededMemoryBeforePushingToEvm(const ptr<CommittedBlock> _block) {
    CHECK_ARGUMENT(_block);

    MONITOR(__CLASS_NAME__, __FUNCTION__)

    try {
        getNode()->getBlockProposalDB()->cleanupUnneededMemoryBeforePushingToEvm(_block);
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


void Schain::pushBlockToExtFace(const ptr<CommittedBlock> &_block) {
    CHECK_ARGUMENT(_block);

    MONITOR2(__CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime())

    checkForExit();

    try {
#ifdef BITE
        auto biteManager = getSchain()->getBiteManager();
        CHECK_STATE(biteManager);
#endif

        auto tv = _block->getTransactionList()->createTransactionVector(
#ifdef BITE
            biteManager
#endif
        );

        // auto next_price = // VERIFY PRICING

        this->pricingAgent->calculatePrice(
            *tv, _block->getTimeStampS(), _block->getTimeStampMs(), _block->getBlockID());

        auto currentPrice = this->pricingAgent->readPrice(_block->getBlockID() - 1);

        // block boundary is the safesf place for exit
        // exit immediately if exit has been requested
        // this will initiate immediate exit and throw ExitRequestedException
        getSchain()->getNode()->checkForExitOnBlockBoundaryAndExitIfNeeded();


#ifdef BITE
        CHECK_STATE(_block->getDecryptedRegularTxFields() || _block->getProposerIndex() == 0)
#endif

        if (extFace) {
            try {
                inCreateBlock = true;

                extFace->createBlock(*tv,
#ifdef BITE
                    _block->getDecryptedTransactions(),
#endif
                    _block->getTimeStampS(), _block->getTimeStampMs(),
                    (__uint64_t) _block->getBlockID(), currentPrice, _block->getStateRoot(),
                    (uint64_t) _block->getProposerIndex()
                );

                inCreateBlock = false;
            } catch (...) {
                inCreateBlock = false;
                throw;
            }
        }

        // block boundary is the safesf place for exit
        // exit immediately if exit has been requested
        // this will initiate immediate exit and throw ExitRequestedException
        getSchain()->getNode()->checkForExitOnBlockBoundaryAndExitIfNeeded();
    } catch (ExitRequestedException &e) {
        throw;
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


void Schain::startConsensus(
    const block_id _blockID, const ptr<BooleanProposalVector> &_proposalVector) { {
        proposalReceiptTime = Time::getCurrentTimeMs() - this->lastCommitTimeMs;
        CHECK_ARGUMENT(_proposalVector);

        MONITOR(__CLASS_NAME__, __FUNCTION__)

        checkForExit();

        CONS_LOG(info, "CONSENSUS_STARTED:PROPOSING: " << _proposalVector->toString());

        CONS_LOG(debug, "Got proposed block set for block:" << to_string( _blockID ));

        CONS_LOG(debug, "StartConsensusIfNeeded BLOCK NUMBER:" << to_string( ( _blockID ) ));

        if (_blockID <= getLastCommittedBlockID()) {
            CONS_LOG(debug, "Too late to start consensus: already committed "
                << to_string( lastCommittedBlockID ));
            return;
        }

        if (_blockID > getLastCommittedBlockID() + 1) {
            CONS_LOG(debug, "Consensus is in the future" << to_string( lastCommittedBlockID ));
            return;
        }
    }


    CHECK_STATE(blockConsensusInstance);
    CHECK_STATE(_proposalVector);

    auto message = make_shared<ConsensusProposalMessage>(*this, _blockID,
#ifdef BITE
    getNode()->getCurrentEpochId(),
#endif
    _proposalVector);

    auto envelope = make_shared<InternalMessageEnvelope>(ORIGIN_EXTERNAL, message, *this);

    CONS_LOG(debug, "Starting consensus for block id:" << to_string( _blockID ));
    postMessage(envelope);
}


void Schain::blockProposalReceiptTimeoutArrived(block_id _blockID) {
    MONITOR(__CLASS_NAME__, __FUNCTION__)

    try {
        if (_blockID <= getLastCommittedBlockID())
            return;

        auto pv = getNode()->getDaProofDB()->getCurrentProposalVector(_blockID);

        CHECK_STATE(pv);

        // try starting consensus. It may already have been started due to
        // block proposals received
        tryStartingConsensus(pv, _blockID);
    } catch (ExitRequestedException &e) {
        throw;
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


void Schain::daProofArrived(const ptr<DAProof> &_daProof) {
    CHECK_ARGUMENT(_daProof);

    MONITOR(__CLASS_NAME__, __FUNCTION__)

    try {
        if (_daProof->getBlockId() <= getLastCommittedBlockID())
            return;

        // this will add the DAProof to DB. If there are enough DAProofs in DB
        // to start binary consensus, this will return binary proposal vector of 1s and 0s
        auto pv =
                addDAProofToDBAndCalculateProposalVectorIfItsTimeToStartBinaryConsensus(_daProof);


        if (pv != nullptr) {
            auto bid = _daProof->getBlockId();

            // try starting consensus. It may already have been started due to
            // block proposal receipt timeout
            tryStartingConsensus(pv, bid);
        }
    } catch (ExitRequestedException &e) {
        throw;
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}

// Consensus is started after 2/3 N + 1 proposals are received, or BlockProposalTimeout is
// reached
void Schain::tryStartingConsensus(const ptr<BooleanProposalVector> &pv, const block_id &bid) {
    auto needToStartConsensus =
            getNode()->getProposalVectorDB()->trySavingProposalVector(bid, pv);
    if (needToStartConsensus)
        startConsensus(bid, pv);
}


void Schain::proposedBlockArrived(const ptr<BlockProposal> &_proposal) {
    MONITOR(__CLASS_NAME__, __FUNCTION__)
    CHECK_STATE(_proposal);

    if (_proposal->getBlockID() <= getLastCommittedBlockID())
        return;

    CHECK_STATE(_proposal->getSignature() != "");

    getNode()->getBlockProposalDB()->addBlockProposal(_proposal);
}


block_id Schain::readLastCommittedBlockIDFromDb() {
    return getNode()->getBlockDB()->readLastCommittedBlockID();
}

void Schain::updateInternalChainInfo(block_id _lastCommittedBlockID) {
    getNode()->getInternalInfoDB()->updateInternalChainInfo(_lastCommittedBlockID);
}


void Schain::bootstrap(block_id _lastCommittedBlockID, uint64_t _lastCommittedBlockTimeStamp,
                       uint64_t _lastCommittedBlockTimeStampMs) {
    // should be called only once
    CHECK_STATE(!bootStrapped.exchange( true ));

    updateInternalChainInfo(_lastCommittedBlockID);


    CONS_LOG(info, "Bootstrapping consensus ...");

    auto lastCommittedBlockIDInConsensus = readLastCommittedBlockIDFromDb();

    CONS_LOG(info,
        "Last committed block in consensus:" << to_string( lastCommittedBlockIDInConsensus ));

    CONS_LOG(info, "Last committed block in skaled:" << to_string( _lastCommittedBlockID ));


    CONS_LOG(info, "Check the consensus database for corruption ...");
    fixCorruptStateIfNeeded(lastCommittedBlockIDInConsensus);

    checkForExit();


    // catch situations that should never happen


    if (lastCommittedBlockIDInConsensus > _lastCommittedBlockID + 128) {
        CONS_LOG(critical,
            "CRITICAL ERROR: consensus has way more blocks than skaled. This should never "
            "happen,"
            "since consensus passes blocks to skaled.");
        BOOST_THROW_EXCEPTION(InvalidStateException(
            "_lastCommittedBlockIDInConsensus > _lastCommittedBlockID + 128", __CLASS_NAME__ ));
    }


    if (lastCommittedBlockIDInConsensus < _lastCommittedBlockID) {
        CONS_LOG(critical,
            "CRITICAL ERROR: last committed block in consensus is smaller than"
            " last committed block in skaled. This can never happen because consensus passes "
            "blocks to skaled");

        BOOST_THROW_EXCEPTION(InvalidStateException(
            "_lastCommittedBlockIDInConsensus < lastCommittedBlockID in EVM", __CLASS_NAME__ ));
    }


    // Step 0: recover missing timestamp fields from consensus DB block if needed.
    // For test/continue startup we may only know the last committed block id.
    if ( _lastCommittedBlockID > 0 &&
         ( _lastCommittedBlockTimeStamp == 0 || _lastCommittedBlockTimeStampMs == 0 ) ) {
        auto block = getNode()->getBlockDB()->getBlock(_lastCommittedBlockID, getCryptoManager());
        if (block) {
            if ( _lastCommittedBlockTimeStamp == 0 ) {
                _lastCommittedBlockTimeStamp = block->getTimeStampS();
            }
            if ( _lastCommittedBlockTimeStampMs == 0 ) {
                _lastCommittedBlockTimeStampMs = block->getTimeStampMs();
            }
        };
    }


    // Step 1: solve block id  mismatch. Consensus may have more blocks than skaled
    // this can happen in case skaled crashed , can also happen when starting from a snapshot

    if (lastCommittedBlockIDInConsensus > _lastCommittedBlockID) {
        // consensus has several more blocks than skaled
        // This happens when starting from a snapshot
        // Since the snapshot is taken just before a block is processed
        // or after multiple skaled crashes
        // process these blocks


        CONS_LOG(warn,
            "Consensus has more blocks than skaled. This should not happen normally since "
            "consensus passes"
            "blocks to skaled.  Skaled may have crashed in the past.");

        while (lastCommittedBlockIDInConsensus > _lastCommittedBlockID)

            try {
#ifdef BITE
                bool isBite2PatchEnabledForBlock = bite2Patch( _lastCommittedBlockTimeStamp );
#endif
                auto block = getNode()->getBlockDB()->getBlock(
                    _lastCommittedBlockID + 1, getCryptoManager() );
                CHECK_STATE2(block, "No block in consensus, repair needed");
#ifdef BITE
                auto reencryptionSignature = block->getReencryptionThresholdSig();
                if ( isBite2PatchEnabledForBlock ) {
                    CHECK_STATE2( reencryptionSignature.has_value(),
                        "BITE2 patch is enabled but reencryption signature is missing for replayed block " +
                            to_string( (uint64_t) block->getBlockID() ) );
                    CHECK_STATE2( !reencryptionSignature->empty(),
                        "BITE2 patch is enabled but reencryption signature is empty for replayed block " +
                            to_string( (uint64_t) block->getBlockID() ) );

                    auto random = Schain::calculateRandomFromSignatureString( *reencryptionSignature );
                    getNode()->getRandomDB()->writeDomainRandom(
                        blockconsensus::REENCRYPTION_RANDOM_DOMAIN, block->getBlockID(), random );
                }
                else {
                    CHECK_STATE2( !reencryptionSignature.has_value(),
                        "BITE2 patch is not enabled but reencryption signature is present for replayed block " +
                            to_string( (uint64_t) block->getBlockID() ) );
                }
#endif
                pushBlockToExtFace(block);
                _lastCommittedBlockID = _lastCommittedBlockID + 1;
                _lastCommittedBlockTimeStamp = block->getTimeStampS();
                _lastCommittedBlockTimeStampMs = block->getTimeStampMs();
                CONS_LOG(info, "Pushed block to skaled:" << _lastCommittedBlockID);
            } catch ( exception& e ) {
                // Cant read the block from db, may be it is corrupt in the  snapshot
                CONS_LOG(err,
                    "Bootstrap could not read block "
                        << (uint64_t) ( _lastCommittedBlockID + 1 )
                        << " from db. Repair. Exception: " << e.what());
                SkaleException::logNested(e);
                // The block will be hopefully pulled by catchup
            } catch (...) {
                // Cant read the block from db, may be it is corrupt in the  snapshot
                CONS_LOG(err,
                    "Bootstrap could not read block "
                        << (uint64_t) ( _lastCommittedBlockID + 1 )
                        << " from db. Repair. Exception: unknown");
                // The block will be hopefully pulled by catchup
            }
    }

    MONITOR2(__CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime())

    // Step 2 : now bootstrap

    CONS_LOG(info, "Starting normal boostrap ...");

    try {
        bootstrapBlockID = (uint64_t) _lastCommittedBlockID;
        CHECK_STATE(_lastCommittedBlockTimeStamp < ( uint64_t ) 2 * MODERN_TIME);

        TimeStamp stamp(_lastCommittedBlockTimeStamp, _lastCommittedBlockTimeStampMs);
        initLastCommittedBlockInfo((uint64_t) _lastCommittedBlockID, stamp);


        CONS_LOG(info, "Jump starting the system with block:" << to_string( _lastCommittedBlockID ));

        if (getLastCommittedBlockID() == 0)
            this->pricingAgent->calculatePrice(ConsensusExtFace::Transactions(), 0, 0, 0);

        isStateInitialized = true;


        if (getNode()->isSyncOnlyNode())
            return;

        {
            lock_guard<timed_mutex> lock((blockProcessMutex));
            auto emptyBlockInterval = getNode()->getEmptyBlockIntervalMs();
            // do not wait much for the first block after start
            // otherwise bootStrapAll() can block node start
            getNode()->setEmptyBlockIntervalMs(50);
            proposeNextBlock(false);
            getNode()->setEmptyBlockIntervalMs(emptyBlockInterval);
            CONS_LOG(info, "Successfully proposed block in boostrap");
        }


        ifIncompleteConsensusDetectedRestartAndRebroadcastAllMessagesForCurrentBlock();
        CONS_LOG(info, "Successfully completed boostrap");
    } catch (exception &e) {
        SkaleException::logNested(e);
        return;
    }
}

void Schain::ifIncompleteConsensusDetectedRestartAndRebroadcastAllMessagesForCurrentBlock() {
    auto proposalVector = getNode()->getProposalVectorDB()->getVector(lastCommittedBlockID + 1);
    if (proposalVector) {
        startConsensus(lastCommittedBlockID + 1, proposalVector);
        CONS_LOG(info, "Incompleted consensus detected.");

        auto messages = getNode()->getOutgoingMsgDB()->getMessages(lastCommittedBlockID + 1);
        CHECK_STATE(messages);
        CONS_LOG(info, "Rebroadcasting " << to_string( messages->size() ) << " messages for block "
            << to_string( lastCommittedBlockID + 1 ));
        for (auto &&m: *messages) {
            getNode()->getNetwork()->rebroadcastMessage(m);
        }
    }
}

void Schain::rebroadcastAllMessagesForCurrentBlock() {
    auto messages = getNode()->getOutgoingMsgDB()->getMessages(lastCommittedBlockID + 1);
    CHECK_STATE(messages);
    CONS_LOG(info, "Rebroadcasting " << to_string( messages->size() ) << " messages for block "
        << to_string( lastCommittedBlockID + 1 ));
    for (auto &&m: *messages) {
        getNode()->getNetwork()->rebroadcastMessage(m);
    }
}


void Schain::healthCheck() {
    std::unordered_set<uint64_t> connections;
    setHealthCheckFile(1);

    auto beginTime = Time::getCurrentTimeSec();
    auto lastWarningPrintTimeSec = 0;

    CONS_LOG(info, "Waiting to connect to peers (could be up to two minutes)");

    // If the node is part of the chain, we do getNodeCount() - 1
    // health check connections, since the node does not connect to itself.
    // A sync-check node can have a total of getNodeCount() health check connections.
    auto countOfNodesToCheck = getNode()->isSyncOnlyNode() ? getNodeCount() : (getNodeCount() - 1);

    while (connections.size() < countOfNodesToCheck) {
        // will optimistically wait for all nodes.
        // if not all nodes are present, will be satisfied by 2/3 nodes

        if (3 * (connections.size() + 1) >= 2 * getNodeCount()) {
            if (Time::getCurrentTimeSec() - beginTime >
                HEALTH_CHECK_TIME_TO_WAIT_FOR_ALL_NODES_SEC) {
                break;
            }
        }

        // If the health check has been runnning for a long time and one could not connect to
        // 2/3 nodes skaled will restart
        if (Time::getCurrentTimeSec() - beginTime > HEALTHCHECK_ON_START_RETRY_TIME_SEC) {
            setHealthCheckFile(0);
            CONS_LOG(err, "Coult not connect to 2/3 of peers");
            exit(110);
        }

        // check if it is time to print a warning again and print it
        if (Time::getCurrentTimeSec() - lastWarningPrintTimeSec >
            HEALTHCHECK_ON_START_TIME_BETWEEN_WARNINGS_SEC) {
            CONS_LOG(warn, "Coult not connect to 2/3 of peers. Retrying ...");
            string aliveNodeIndices = "Alive node indices:";

            for (auto &index: connections) {
                aliveNodeIndices += to_string(index) + ":";
            };

            CONS_LOG(warn, aliveNodeIndices);

            lastWarningPrintTimeSec = Time::getCurrentTimeSec();
        }


        if (getNode()->isExitRequested()) {
            BOOST_THROW_EXCEPTION(ExitRequestedException( __CLASS_NAME__ ));
        }

        usleep(TIME_BETWEEN_STARTUP_HEALTHCHECK_RETRIES_SEC * 1000000);

        for (int i = 1; i <= getNodeCount(); i++) {
            if (i != (getSchainIndex()) && !connections.count(i)) {
                try {
                    if (getNode()->isExitRequested()) {
                        BOOST_THROW_EXCEPTION(ExitRequestedException( __CLASS_NAME__ ));
                    }

                    auto port =
                            (getNode()->isSyncOnlyNode() ? port_type::CATCHUP : port_type::PROPOSAL);

                    auto socket = make_shared<ClientSocket>(*this, schain_index(i), port);
                    CONS_LOG(debug, "Health check: connected to peer");
                    getIo()->writeMagic(socket, true);
                    connections.insert(i);
                } catch (ExitRequestedException &) {
                    throw;
                } catch (std::exception &e) {
                }
            }
        }
    }

    CONS_LOG(info, "Successfully connected to two thirds of peers");

    setHealthCheckFile(2);
}

void Schain::daProofSigShareArrived(
    const ptr<ThresholdSigShare> &_sigShare, const ptr<BlockProposal> &_proposal) {
    MONITOR(__CLASS_NAME__, __FUNCTION__)

    checkForExit();

    CHECK_ARGUMENT(_sigShare != nullptr);
    CHECK_ARGUMENT(_proposal != nullptr);


    try {
        auto proof =
                getNode()->getDaSigShareDB()->addAndMergeSigShareAndVerifySig(_sigShare, _proposal);
        if (proof != nullptr) {
            getSchain()->daProofArrived(proof);
            blockProposalClient->enqueueItem(proof);
        }
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        CONS_LOG(err, "Could not add/merge sig");
        throw_with_nested(InvalidStateException("Could not add/merge sig", __CLASS_NAME__));
    }
}


void Schain::constructServers(const ptr<Sockets> &_sockets) {
    MONITOR(__CLASS_NAME__, __FUNCTION__)

    catchupServerAgent = make_shared<CatchupServerAgent>(*this, _sockets->catchupSocket);


    if (getNode()->isSyncOnlyNode())
        return;

    blockProposalServerAgent =
            make_shared<BlockProposalServerAgent>(*this, _sockets->blockProposalSocket);
}

ptr<BlockProposal> Schain::createDefaultEmptyBlockProposal(block_id _blockId
#ifdef BITE
    , epoch_id _epochID
#endif
) {
    TimeStamp newStamp; {
        lock_guard<mutex> l(lastCommittedBlockInfoMutex);
        newStamp = lastCommittedBlockTimeStamp.incrementByMs();
    }

    return make_shared<ReceivedBlockProposal>(
        *this, _blockId,
#ifdef BITE
        _epochID,
#endif
        newStamp.getS(), newStamp.getMs(), 0);
}


void Schain::finalizeDecidedAndSignedBlock(block_id _blockId, schain_index _proposerIndex,
                                           const ptr<ThresholdSignature> &_thresholdSig
#ifdef BITE
                                           , const ptr<ThresholdSignature> &_reencryptionThresholdSig
#endif
    ) {

    checkForExit();
#ifdef BITE
    getFinalizationExecutor()->add([_blockId, _proposerIndex, _thresholdSig, 
        _reencryptionThresholdSig, 
        this]() {
#endif
        logThreadLocal_ = getNode()->getLog();
        finalizeDecidedAndSignedBlockInThread(_blockId, _proposerIndex, _thresholdSig 
#ifdef BITE
            , _reencryptionThresholdSig
#endif
        );
#ifdef BITE
    });
#endif
};


bool Schain::haveDAProof(block_id _blockId, schain_index _proposerIndex) {
    auto daProofSig = getNode()->getDaProofDB()->getDASig(_blockId, _proposerIndex);
    return !daProofSig.empty();
}


bool Schain::haveProposal(block_id _blockId, schain_index _proposerIndex) {
    auto proposal = getNode()->getBlockProposalDB()->getBlockProposal(_blockId, _proposerIndex);
    return proposal != nullptr;
}


bool Schain::haveAllElementsToFinalizeBlock(block_id _blockId, schain_index _proposerIndex) {
    // to finalize a block and pass it to skaled we need
    // blockproposal, da proof, and decryption shares

    return haveProposal(_blockId, _proposerIndex) && haveDAProof(_blockId, _proposerIndex)
#ifdef BITE
           && getNode()->getTEDecryptionDB()->isEnoughForeignShares(_blockId)
#endif
            ;
}

#include <libBLS/bls/BLSPublicKeyShare.h>

void Schain::finalizeDecidedAndSignedBlockInThread(block_id _blockId, schain_index _proposerIndex,
                                                   const ptr<ThresholdSignature> &_thresholdSig
#ifdef BITE
                                                   , const ptr<ThresholdSignature> &_reencryptionThresholdSig
#endif
    ) {

    CHECK_ARGUMENT(_thresholdSig != nullptr);


    MONITOR2(__CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime())



    proposalStageFinishTimeMs = Time::getCurrentTimeMs();
    blockFinalizationStartTimeMs = Time::getCurrentTimeMs();
    if ( _blockId <= getLastCommittedBlockID() ) {
        CONS_LOG( debug, "Ignoring old block decide, already got this through catchup: BID:"
                        << to_string( _blockId ) << ":PRP:" << to_string( _proposerIndex ) );

        return;
    }



    try {
        if (_proposerIndex == 0) {
            // default empty block
            blockCommitArrived(_blockId, _proposerIndex, _thresholdSig, 
#ifdef BITE
                _reencryptionThresholdSig, 
#endif
                nullptr
#ifdef BITE
            , make_shared<DecryptedAESKeyList>(), DecryptedTransactions()
#endif
            );
            return;
        }

        if (!haveAllElementsToFinalizeBlock(_blockId, _proposerIndex) ||
            // force download  - this switch is for testing only
            getNode()->getTestConfig()->isFinalizationDownloadOnly()
        ) {
            // Dowload missing objects - proposal, daProof, and decryption shares
            // Note that due to the BLS signature proof, 2t hosts out of 3t + 1 total are
            // guaranteed to posSess the proposal
            auto newDownloaderAgent = make_shared<BlockFinalizeDownloader>(this, _blockId,
#ifdef BITE
            getNode()->getCurrentEpochId(),
#endif
            _proposerIndex);

            // at this point the destructor of the previous agent will be called
            // this will make all its threads to exit
            downloaderAgent =  newDownloaderAgent;

            const string msg = "Finalization download:" + to_string(_blockId) + ":" +
                                   to_string(_proposerIndex);

            MONITOR(__CLASS_NAME__, msg.c_str());
                // This will complete successfully also if block arrives through catchup
            auto completedDownload = downloaderAgent->downloadProposalDAProofAndDecryptions();
                // if null is returned it means that catchup happened first and
                // the block will be processed through catchup
            if (!completedDownload) {
                    // catchup happened
                return;
            }
            CHECK_STATE(haveAllElementsToFinalizeBlock(_blockId, _proposerIndex));
        }


        auto proposal = getNode()->getBlockProposalDB()->getBlockProposal(_blockId, _proposerIndex);
        CHECK_STATE(proposal);
        blockFinalizationFinishTimeMs = Time::getCurrentTimeMs();

#ifdef BITE
        // Do not hard-require the local SGX share: if it failed (e.g. SGX node
        // unreachable), a full threshold of foreign shares is sufficient to
        // reconstruct the AES keys and finalize the block.
        auto myDecryptionShares = proposal->getMyDecryptionShares();
        if (myDecryptionShares) {
            getNode()->getTEDecryptionDB()->addDecryptionShares(myDecryptionShares);
        }

        auto count =
                getNode()->getTEDecryptionDB()->getDecryptionsCount(_blockId);

        CHECK_STATE(count >= getRequiredSigners())

        auto encryptedAESKeys = proposal->getTransactionCiphertexts();

        CHECK_STATE(encryptedAESKeys);

        auto keys = getNode()->getTEDecryptionDB()->mergeAESKeys(proposal->getBlockID(), encryptedAESKeys);

        CHECK_STATE(keys);

        auto transactions = proposal->getTransactionList();
        bool isBite2PatchEnabledForBlock = bite2Patch( getLastCommittedBlockTimeStamp().getS() );
        auto decryptedTransactions = getBiteManager()->verifyAndDecryptTransactionList(
            *transactions, (*keys), proposal->getEpochID()
            , isBite2PatchEnabledForBlock 
        );
#endif // BITE

        auto daProofSig = getNode()->getDaProofDB()->getDASig( _blockId, _proposerIndex );
        auto hash = proposal->getHash();
        auto daSig = getSchain()->getCryptoManager()->verifyDAProofThresholdSig(
            hash, daProofSig, _blockId, proposal->getTimeStampS() );

        blockCommitArrived(_blockId, _proposerIndex, _thresholdSig, 
#ifdef BITE
            _reencryptionThresholdSig,
#endif
            daSig
#ifdef BITE
                           , keys, decryptedTransactions
#endif
        );
    } catch (ExitRequestedException &) {
        return;
    } catch (exception &e) {
        SkaleException::logNested(e);
        CONS_LOG(critical, "Could not finalizeDecidedAndSignedBlock. Hopefully catchup will work.");
    } catch (...) {
        CONS_LOG(critical, "Unknown exception in finalizeDecidedAndSignedBlock");
        CONS_LOG(critical, "Could not finalizeDecidedAndSignedBlock. Hopefully catchup will work.");
    }
}

// empty constructor is used for tests
Schain::Schain() : Agent() {
}

bool Schain::fixCorruptStateIfNeeded(block_id _lastCommittedBlockID) {
    block_id nextBlock = _lastCommittedBlockID + 1;
    if (getNode()->getBlockDB()->unfinishedBlockExists(nextBlock)) {
        CONS_LOG(warn,
            "Corrupt consensus database has been repaired successfully."
            "Starting from repaired consensus database.");
        return true;
    }
    return false;
}

void Schain::startStatusServer() {
    if (!s) {
        httpserver = make_shared<jsonrpc::HttpServer>(
            (int) ((uint16_t) getNode()->getBasePort() + STATUS), "", "", "", 1);
        s = make_shared<StatusServer>(this, *httpserver, jsonrpc::JSONRPC_SERVER_V1V2);
    }

#ifdef CONSENSUS_DEMO
    CHECK_STATE( s );
    CONS_LOG( info, "Starting status server ..." );
    CHECK_STATE( s->StartListening() );
    CONS_LOG( info, "Successfully started status server ..." );
#endif
}

void Schain::stopStatusServer() {
    if (s)
        s->StopListening();
}

uint64_t Schain::getBlockSizeAverage() const {
    return blockSizeAverage;
}

uint64_t Schain::getBlockTimeAverageMs() const {
    return blockTimeAverageMs;
}

uint64_t Schain::getTpsAverage() const {
    return tpsAverage;
}

void Schain::addDeadNode(uint64_t _schainIndex, uint64_t _checkTime) {
    CHECK_STATE(_schainIndex > 0);
    CHECK_STATE(_schainIndex <= getNodeCount()); {
        lock_guard<mutex> l(deadNodesLock);
        deadNodes[_schainIndex] = _checkTime;
    }
}

void Schain::markAliveNode(uint64_t _schainIndex) {
    CHECK_STATE(_schainIndex > 0);
    CHECK_STATE(_schainIndex <= getNodeCount());

    bool wasDead = false; {
        lock_guard<mutex> l(deadNodesLock);
        wasDead = deadNodes.erase(_schainIndex) > 0;
    }

    if (wasDead) {
        CONS_LOG(info, "Node " + to_string( _schainIndex ) + " is now alive");
    }
}

uint64_t Schain::getDeathTimeMs(uint64_t _schainIndex) {
    CHECK_STATE(_schainIndex > 0);
    CHECK_STATE(_schainIndex <= getNodeCount()); {
        lock_guard<mutex> l(deadNodesLock);
        if (deadNodes.count(_schainIndex) == 0) {
            return 0;
        } else {
            return deadNodes.at(_schainIndex);
        }
    }
}

ptr<ofstream> Schain::getVisualizationDataStream() {
    lock_guard<mutex> l(vdsMutex);
    if (!visualizationDataStream) {
        visualizationDataStream = make_shared<ofstream>();
        visualizationDataStream->exceptions(std::ofstream::badbit | std::ofstream::failbit);
        auto t = Time::getCurrentTimeMs();
        auto fileName = "/tmp/consensusv_" + to_string(t) + ".data";
        visualizationDataStream->open(fileName, ios_base::trunc);
    }
    return visualizationDataStream;
}

void Schain::writeToVisualizationStream(string &_s) {
    lock_guard<mutex> l(vdsMutex);
    auto stream = getVisualizationDataStream();
    stream->write(_s.c_str(), _s.size());
}


u256 Schain::getRandomForBlockId(block_id _blockId) {
    // Ensure the block has already been committed to the database
    CHECK_STATE(_blockId <= readLastCommittedBlockIDFromDb());
    auto block = getNode()->getBlockDB()->getBlock( _blockId, getCryptoManager() );
    
    CHECK_STATE(block);
    return calculateRandomFromSignatureString( block->getThresholdSig() );
}

u256 Schain::calculateRandomFromSignatureString( const string& _signature ) {
    CHECK_ARGUMENT( !_signature.empty() )

    auto data = make_shared< vector< uint8_t > >();
    data->reserve( _signature.size() );
    for ( uint64_t i = 0; i < _signature.size(); i++ ) {
        data->push_back( ( uint8_t ) _signature.at( i ) );
    }

    auto hash = BLAKE3Hash::calculateHash( data );
    return u256( "0x" + hash.toHex() );
}

#ifdef BITE
u256 Schain::getReencryptionRandomForBlockId( block_id _blockId ) {
    // Ensure the block has already been committed to the database
    CHECK_STATE(_blockId <= readLastCommittedBlockIDFromDb());
    return getNode()->getRandomDB()->readDomainRandom(
        blockconsensus::REENCRYPTION_RANDOM_DOMAIN, _blockId );
}
#endif

ptr<ofstream> Schain::visualizationDataStream = nullptr;

#ifndef FAIR
const ptr< OracleResultAssemblyAgent >& Schain::getOracleResultAssemblyAgent() const {
    return oracleResultAssemblyAgent;
}
#endif

void Schain::addBlockErrorAnalyzer(ptr<BlockErrorAnalyzer> _blockErrorAnalyzer) {
    {
        LOCK(blockErrorAnalyzersMutex)
        blockErrorAnalyzers.push_back(_blockErrorAnalyzer);
    }
}


void Schain::analyzeErrors(ptr<CommittedBlock> _block) {
    vector<ptr<BlockErrorAnalyzer> > analyzers; {
        LOCK(blockErrorAnalyzersMutex)
        analyzers = blockErrorAnalyzers;
        blockErrorAnalyzers = vector<ptr<BlockErrorAnalyzer> >();
    }

    for (auto &&analyzer: analyzers) {
        analyzer->analyze(_block);
    }
}


mutex Schain::vdsMutex;

// this function is called on arrival of each DA proof
// if it is time to make binary proposals it will return a vector of 0s and 1s
// for normal consensus it will happen when 2t+1 DA proofs  arrive (which is 11)
// for optimized consensus it will happen when a DA proof from the previous winner arrives
ptr<BooleanProposalVector>
Schain::addDAProofToDBAndCalculateProposalVectorIfItsTimeToStartBinaryConsensus(
    const ptr<DAProof> &_daProof) {
    ptr<BooleanProposalVector> pv;

    if (getOptimizerAgent()->doOptimizedConsensus(
        _daProof->getBlockId(), getLastCommittedBlockTimeStamp().getS())) {
        // when we do optimized block consensus only the previous winner
        // proposes and provides da proof
        // proposals from other nodes, if sent made by mistake, are ignored
        auto lastWinner = getOptimizerAgent()->getPreviousWinner(_daProof->getBlockId());
        if (_daProof->getProposerIndex() == lastWinner) {
            getNode()->getDaProofDB()->addDAProof(_daProof);
            pv = make_shared<BooleanProposalVector>(getNodeCount(), lastWinner);
        }
    } else {
        // do things regular way
        // the binary proposal vector is formed and the consensus is started when
        // 2/3 of nodes  (11) submit a da proof
        pv = getNode()->getDaProofDB()->addDAProof(_daProof);
    }
    return pv;
}

// returns true if fastConsensusPatch ie enabled
bool Schain::fastConsensusPatchEnabled(uint64_t
#ifndef BITE
        _blockTimeStampSec
#endif
) {
#ifdef BITE
    return true; //
#else
    return fastConsensusPatchTimestamp != 0 && _blockTimeStampSec >= fastConsensusPatchTimestamp;
#endif
}

// macro to set patchstamp variable from connfig
#define SET_TIMESTAMP_FROM_CONFIG( __TIMESTAMP_NAME__ )                \
    {                                                                  \
        auto& timestamps = getNode()->getPatchTimestamps();            \
        if ( timestamps.count( #__TIMESTAMP_NAME__ ) > 0 ) {           \
            __TIMESTAMP_NAME__ = timestamps.at( #__TIMESTAMP_NAME__ ); \
        }                                                              \
    }

// set all timestamp values from config
void Schain::setTimeStampValuesFromConfig() {
    SET_TIMESTAMP_FROM_CONFIG(verifyDaSigsPatchTimestamp)
    SET_TIMESTAMP_FROM_CONFIG(fastConsensusPatchTimestamp)
    SET_TIMESTAMP_FROM_CONFIG(verifyBlsSyncPatchTimestamp)
#ifdef BITE
    SET_TIMESTAMP_FROM_CONFIG(bite2PatchTimestamp)
    // Backward compatibility for configs using legacy key casing.
    if ( bite2PatchTimestamp == 0 ) {
        auto& timestamps = getNode()->getPatchTimestamps();
        if ( timestamps.count( "bite2PatchTimestamp" ) > 0 ) {
            bite2PatchTimestamp = timestamps.at( "bite2PatchTimestamp" );
        }
    }
#endif
}

uint64_t Schain::getProposalStageTimeMs() {
    uint64_t proposalStageTimeMs = proposalStageFinishTimeMs - proposalStageStartTimeMs;
    proposalStageFinishTimeMs = proposalStageStartTimeMs = 0;
    return proposalStageTimeMs;
}

uint64_t Schain::getBlockFinalizationStageTimeMs() {
    uint64_t blockFinalizationStageTimeMs = blockFinalizationFinishTimeMs - blockFinalizationStartTimeMs;
    blockFinalizationStartTimeMs = blockFinalizationFinishTimeMs = 0;
    return blockFinalizationStageTimeMs;
}

#ifdef BITE
const shared_ptr< folly::CPUThreadPoolExecutor >& Schain::getFinalizationExecutor() const {
    CHECK_STATE(finalizationExecutor);
    return finalizationExecutor;
}

void Schain::stopAndDestroyFinalizationExecutor() {
    if ( !finalizationExecutor )
        return;
    finalizationExecutor->stop();
    finalizationExecutor->join();
}
#endif
