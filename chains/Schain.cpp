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


#include "leveldb/db.h"
#include <malloc.h>
#include <sched.h>
#include <unordered_set>

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
#include "network/ClientSocket.h"
#include "network/IO.h"
#include "network/Sockets.h"
#include "network/ZMQSockets.h"
#include "node/NodeInfo.h"
#include "oracle/OracleClient.h"
#include "oracle/OracleMessageThreadPool.h"
#include "oracle/OracleResultAssemblyAgent.h"
#include "oracle/OracleServerAgent.h"
#include "oracle/OracleThreadPool.h"
#include "pricing/PricingAgent.h"
#include "protocols/ProtocolInstance.h"
#include "protocols/blockconsensus/BlockConsensusAgent.h"


#include "Schain.h"
#include "SchainMessageThreadPool.h"
#include "SchainTest.h"
#include "TestConfig.h"
#include "crypto/CryptoManager.h"
#include "crypto/ThresholdSigShare.h"
#include "crypto/bls_include.h"
#include "db/BlockDB.h"
#include "db/CacheLevelDB.h"
#include "db/ProposalHashDB.h"
#include "libBLS/bls/BLSPrivateKeyShare.h"
#include "monitoring/LivelinessMonitor.h"
#include "monitoring/TimeoutAgent.h"
#include "pendingqueue/TestMessageGeneratorAgent.h"

template < class M >
class try_lock_timed_guard {
private:
    M& mtx_;
    std::atomic_bool was_locked_;

    bool try_lock( const size_t nNumberOfMilliseconds ) {
        auto now = std::chrono::steady_clock::now();
        if ( mtx_.try_lock_until( now + std::chrono::milliseconds( nNumberOfMilliseconds ) ) )
            return true;  // was locked
        return false;
    }

public:
    explicit try_lock_timed_guard( M& mtx, const size_t nNumberOfMilliseconds = 1000 )
        : mtx_( mtx ), was_locked_( false ) {
        was_locked_ = try_lock( nNumberOfMilliseconds );
    }

    ~try_lock_timed_guard() {
        if ( was_locked_ )
            mtx_.unlock();
    }

    bool was_locked() const { return was_locked_; }
};

void Schain::postMessage( const ptr< MessageEnvelope >& _me ) {
    CHECK_ARGUMENT( _me );

    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    checkForExit();

    CHECK_STATE( ( uint64_t ) _me->getMessage()->getBlockId() != 0 );
    {
        lock_guard< mutex > l( messageMutex );
        messageQueue.push( _me );
        messageCond.notify_all();
    }
}


void Schain::messageThreadProcessingLoop( Schain* _sChain ) {
    CHECK_ARGUMENT( _sChain );

    setThreadName( "msgThreadProcLoop", _sChain->getNode()->getConsensusEngine() );

    _sChain->waitOnGlobalStartBarrier();

    try {
        _sChain->startTimeMs = Time::getCurrentTimeMs();

        logThreadLocal_ = _sChain->getNode()->getLog();

        queue< ptr< MessageEnvelope > > newQueue;

        while ( !_sChain->getNode()->isExitRequested() ) {
            {
                unique_lock< mutex > mlock( _sChain->messageMutex );
                while ( _sChain->messageQueue.empty() ) {
                    _sChain->messageCond.wait( mlock );
                    if ( _sChain->getNode()->isExitRequested() )
                        return;
                }

                newQueue = _sChain->messageQueue;

                while ( !_sChain->messageQueue.empty() ) {
                    if ( _sChain->getNode()->isExitRequested() )
                        return;

                    _sChain->messageQueue.pop();
                }
            }

            while ( !newQueue.empty() ) {
                if ( _sChain->getNode()->isExitRequested() )
                    return;

                ptr< MessageEnvelope > m = newQueue.front();
                CHECK_STATE( ( uint64_t ) m->getMessage()->getBlockId() != 0 );

                try {
                    _sChain->getBlockConsensusInstance()->routeAndProcessMessage( m );

                } catch ( exception& e ) {
                    LOG( err, "Exception in Schain::messageThreadProcessingLoop" );
                    SkaleException::logNested( e );
                    if ( _sChain->getNode()->isExitRequested() )
                        return;
                }  // catch

                newQueue.pop();
            }
        }
    } catch ( FatalError& e ) {
        SkaleException::logNested( e );
        _sChain->getNode()->initiateApplicationExitOnFatalConsensusError( e.what() );
    }
}


void Schain::startThreads() {
    if ( getNode()->isSyncOnlyNode() ) {
        return;
    }
    CHECK_STATE( consensusMessageThreadPool )
    this->consensusMessageThreadPool->startService();
}

const string& Schain::getSchainName() const {
    return schainName;
}

Schain::Schain( weak_ptr< Node > _node, schain_index _schainIndex, const schain_id& _schainID,
    ConsensusExtFace* _extFace, string& _schainName )
    : Agent( *this, true, true ),
      totalTransactions( 0 ),
      extFace( _extFace ),
      schainID( _schainID ),
      schainName( _schainName ),
      startTimeMs( 0 ),
      consensusMessageThreadPool( new SchainMessageThreadPool( this ) ),
      node( _node ),
      schainIndex( _schainIndex ) {
    lastCommittedBlockTimeStamp = TimeStamp( 0, 0 );

    // construct monitoring, timeout and stuck detection agents early
    monitoringAgent = make_shared< MonitoringAgent >( *this );
    if ( !getNode()->isSyncOnlyNode() ) {
        timeoutAgent = make_shared< TimeoutAgent >( *this );
        stuckDetectionAgent = make_shared< StuckDetectionAgent >( *this );
    }

    maxExternalBlockProcessingTime =
        std::max( 2 * getNode()->getEmptyBlockIntervalMs(), ( uint64_t ) 3000 );

    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    if ( !getNode()->isSyncOnlyNode() ) {
        CHECK_STATE( schainIndex > 0 );
        CHECK_STATE( getNode()->getNodeInfosByIndex()->size() > 0 );
    }

    try {
        this->io = make_shared< IO >( this );


        for ( auto const& iterator : *getNode()->getNodeInfosByIndex() ) {
            if ( iterator.second->getNodeID() == getNode()->getNodeID() ) {
                CHECK_STATE( thisNodeInfo == nullptr && iterator.second != nullptr );
                thisNodeInfo = iterator.second;
            }
        }

        if ( thisNodeInfo == nullptr && !getNode()->isSyncOnlyNode() ) {
            BOOST_THROW_EXCEPTION( EngineInitException(
                "Schain: " + to_string( ( uint64_t ) getSchainID() ) +
                    " does not include current node with IP " + getNode()->getBindIP() +
                    "and node id " + to_string( getNode()->getNodeID() ),
                __CLASS_NAME__ ) );
        }

        CHECK_STATE( getNodeCount() > 0 );

        constructChildAgents();

        startStatusServer();

        string none = SchainTest::NONE;

        blockProposerTest = none;

        getNode()->registerAgent( this );


        if ( getNode()->getPatchTimestamps().count( "verifyDaSigsPatchTimestamp" ) > 0 ) {
            this->verifyDaSigsPatchTimestampS =
                getNode()->getPatchTimestamps().at( "verifyDaSigsPatchTimestamp" );
        }

    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( FatalError( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


// called from constructor so no locks needed
void Schain::constructChildAgents() {
    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    try {
        oracleResultAssemblyAgent = make_shared< OracleResultAssemblyAgent >( *this );
        pricingAgent = make_shared< PricingAgent >( *this );
        catchupClientAgent = make_shared< CatchupClientAgent >( *this );

        cryptoManager = make_shared< CryptoManager >( *this );

        if ( getNode()->isSyncOnlyNode() ) {
            return;
        }

        pendingTransactionsAgent = make_shared< PendingTransactionsAgent >( *this );
        blockProposalClient = make_shared< BlockProposalClientAgent >( *this );

        testMessageGeneratorAgent = make_shared< TestMessageGeneratorAgent >( *this );

        oracleClient = make_shared< OracleClient >( *this );
    } catch ( ... ) {
        throw_with_nested( FatalError( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


void Schain::lockWithDeadLockCheck( const char* _functionName ) {
    while ( !blockProcessMutex.try_lock_for( chrono::seconds( 60 ) ) ) {
        checkForExit();
        LOG( err, "Trying to lock in:" + string( _functionName ) );
    }
}


[[nodiscard]] uint64_t Schain::blockCommitsArrivedThroughCatchup(
    const ptr< CommittedBlockList >& _blockList ) {
    CHECK_ARGUMENT( _blockList );

    auto blocks = _blockList->getBlocks();

    CHECK_STATE( blocks );

    if ( blocks->size() == 0 ) {
        return 0;
    }

    try {
        if ( !blockProcessMutex.try_lock_for( chrono::seconds( 60 ) ) ) {
            // Could not lock for 60 seconds. There is probably a deadlock.
            // Skipping this catchup iteration
            checkForExit();
            LOG( err, "Could not lock in:" + string( __FUNCTION__ ) );
            return 0;
        }

        bumpPriority();

        atomic< uint64_t > committedIDOld = ( uint64_t ) getLastCommittedBlockID();

        CHECK_STATE( blocks->at( 0 )->getBlockID() <= ( uint64_t ) getLastCommittedBlockID() + 1 );

        for ( size_t i = 0; i < blocks->size(); i++ ) {
            checkForExit();

            auto block = blocks->at( i );

            CHECK_STATE( block );

            if ( ( uint64_t ) block->getBlockID() == ( getLastCommittedBlockID() + 1 ) ) {
                CHECK_STATE( getLastCommittedBlockTimeStamp() < block->getTimeStamp() );
                processCommittedBlock( block );
            }
        }

        uint64_t result = 0;

        if ( committedIDOld < getLastCommittedBlockID() ) {
            LOG( info, "CATCHUP_PROCESSED_BLOCKS:COUNT: " +
                           to_string( getLastCommittedBlockID() - committedIDOld ) );
            result = ( ( uint64_t ) getLastCommittedBlockID() ) - committedIDOld;
            if ( !getNode()->isSyncOnlyNode() )
                proposeNextBlock();
        }

        unbumpPriority();

        blockProcessMutex.unlock();
        return result;
    } catch ( ... ) {
        unbumpPriority();
        blockProcessMutex.unlock();
        throw;
    }
}

const atomic< bool >& Schain::getIsStateInitialized() const {
    return isStateInitialized;
}

bool Schain::verifyDASigsPatch( uint64_t _blockTimeStampS ) {
    return verifyDaSigsPatchTimestampS != 0 && _blockTimeStampS >= verifyDaSigsPatchTimestampS;
}


void Schain::blockCommitArrived( block_id _committedBlockID, schain_index _proposerIndex,
    const ptr< ThresholdSignature >& _thresholdSig, ptr< ThresholdSignature > _daSig ) {
    MONITOR2( __CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime() )

    CHECK_ARGUMENT( _thresholdSig )
    CHECK_ARGUMENT( _daSig || _proposerIndex == 0 )

    // wait until the schain state is fully initialized and startup
    // otherwise last committed block id is not fully initialized and the chain can not accept
    // catchup blocks
    while ( !getSchain()->getIsStateInitialized() ) {
        usleep( 500 * 1000 );
        LOG( info, "Waiting for boostrap to complete ..." );
    }

    // no regular block commits happen for sync nodes
    CHECK_STATE( !getNode()->isSyncOnlyNode() );

    checkForExit();

    try {
        lockWithDeadLockCheck( __FUNCTION__ );


        if ( _committedBlockID <= getLastCommittedBlockID() )
            return;


        CHECK_STATE( _committedBlockID == ( getLastCommittedBlockID() + 1 ) ||
                     getLastCommittedBlockID() == 0 );

        bumpPriority();


        ptr< BlockProposal > committedProposal = nullptr;

        if ( _proposerIndex > 0 ) {
            committedProposal = getNode()->getBlockProposalDB()->getBlockProposal(
                _committedBlockID, _proposerIndex );

        } else {
            committedProposal = createDefaultEmptyBlockProposal( _committedBlockID );
        }

        CHECK_STATE( committedProposal );

        auto newCommittedBlock =
            CommittedBlock::makeFromProposal( committedProposal, _thresholdSig, _daSig );

        CHECK_STATE( getLastCommittedBlockTimeStamp() < newCommittedBlock->getTimeStamp() );


        processCommittedBlock( newCommittedBlock );
        proposeNextBlock();

        unbumpPriority();

        blockProcessMutex.unlock();

    } catch ( ExitRequestedException& e ) {
        unbumpPriority();
        blockProcessMutex.unlock();
        throw;
    } catch ( ... ) {
        unbumpPriority();
        blockProcessMutex.unlock();
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


void Schain::checkForExit() {
    if ( getNode()->isExitRequested() ) {
        BOOST_THROW_EXCEPTION( ExitRequestedException( __CLASS_NAME__ ) );
    }
}


// Note: this function must be called with blockProcessing mutex held
void Schain::proposeNextBlock() {
    MONITOR2( __CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime() )

    checkForExit();
    try {
        block_id _proposedBlockID( ( uint64_t ) lastCommittedBlockID + 1 );

        ptr< BlockProposal > myProposal;

        if ( getNode()->getProposalHashDB()->haveProposal( _proposedBlockID, getSchainIndex() ) ) {
            myProposal = getNode()->getBlockProposalDB()->getBlockProposal(
                _proposedBlockID, getSchainIndex() );
        } else {
            auto stamp = getLastCommittedBlockTimeStamp();
            myProposal = pendingTransactionsAgent->buildBlockProposal( _proposedBlockID, stamp );
        }

        CHECK_STATE( myProposal );

        CHECK_STATE( myProposal->getProposerIndex() == getSchainIndex() );
        CHECK_STATE( myProposal->getSignature() != "" );


        proposedBlockArrived( myProposal );

        LOG( debug, "PROPOSING BLOCK NUMBER:" + to_string( _proposedBlockID ) );

        auto db = getNode()->getProposalHashDB();

        db->checkAndSaveHash( _proposedBlockID, getSchainIndex(), myProposal->getHash().toHex() );

        blockProposalClient->enqueueItem( myProposal );

        auto [mySig, ecdsaSig, pubKey, pubKeySig] =
            getSchain()->getCryptoManager()->signDAProof( myProposal );

        CHECK_STATE( mySig );

        // make compiler happy
        ecdsaSig = "";
        pubKey = "";
        pubKeySig = "";

        getSchain()->daProofSigShareArrived( mySig, myProposal );

    } catch ( ExitRequestedException& e ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}

void Schain::bumpPriority() {
    // temporary bump thread priority
    // We'll operate on the currently running thread.
    pthread_t this_thread = pthread_self();
    struct sched_param params;
    // We'll set the priority to the maximum.
    params.sched_priority = sched_get_priority_max( SCHED_FIFO );
    pthread_setschedparam( this_thread, SCHED_FIFO, &params );
}

void Schain::unbumpPriority() {
    struct sched_param params;
    // Set the priority to norm
    pthread_t this_thread = pthread_self();
    params.sched_priority = 0;
    CHECK_STATE( pthread_setschedparam( this_thread, 0, &params ) == 0 )
}


void Schain::saveToVisualization( ptr< CommittedBlock > _block, uint64_t _visualizationType ) {
    CHECK_STATE( _block );


    string info = string( "{" ) + "\"t\":" + to_string( MsgType::MSG_BLOCK_COMMIT ) + "," +
                  "\"b\":" + to_string( Time::getCurrentTimeMs() - getStartTimeMs() ) + "," +
                  "\"s\":" + to_string( getSchain()->getSchainIndex() ) + "," +
                  "\"p\":" + to_string( _block->getProposerIndex() ) + "," +
                  "\"i\":" + to_string( _block->getBlockID() ) + "}\n";


    if ( _visualizationType == 1 )
        Schain::writeToVisualizationStream( info );
}

void Schain::printBlockLog( const ptr< CommittedBlock >& _block ) {
    CHECK_STATE( _block );

    MONITOR2( __CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime() )

    totalTransactions += _block->getTransactionList()->size();

    auto h = _block->getHash().toHex().substr( 0, 8 );

    auto stamp = TimeStamp( _block->getTimeStampS(), _block->getTimeStampMs() );


    stringstream output;

    output << "BLOCK_COMMITED: PRPSR:" << _block->getProposerIndex()
           << ":BID: " << _block->getBlockID()
           << ":ROOT:" << _block->getStateRoot().convert_to< string >() << ":HASH:" << h
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


    if ( !getNode()->isSyncOnlyNode() ) {
        output << ":KNWN:" << pendingTransactionsAgent->getKnownTransactionsSize()
               << ":CONS:" << ServerConnection::getTotalObjects()
               << ":DSDS:" << getSchain()->getNode()->getNetwork()->computeTotalDelayedSends()
               << ":SET:" << CryptoManager::getEcdsaStats()
               << ":SBT:" << CryptoManager::getBLSStats()
               << ":SEC:" << CryptoManager::getECDSATotals()
               << ":SBC:" << CryptoManager::getBLSTotals()
               << ":ZSC:" << getCryptoManager()->getZMQSocketCount()
               << ":EPT:" << lastCommittedBlockEvmProcessingTimeMs;
    }

    output << ":STAMP:" << stamp.toString();

    LOG( info, output.str() );

    // get periodic stats
    static atomic< uint64_t > counter = 1;

    if ( counter % 20 == 0 ) {
        output.str( "" );
        output << "LEVELDB_MEM_STATS:BLOCKS:" << getNode()->getBlockDB()->getMemoryUsed();
        ;
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
        LOG( info, output.str() );
        LOG( info, Utils::getRusage() );
    }

    counter++;
}

void Schain::processCommittedBlock( const ptr< CommittedBlock >& _block ) {
    CHECK_ARGUMENT( _block );
    // process committed block needs to be called why holding main mutex

    MONITOR2( __CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime() )

    checkForExit();

    if ( getSchain()->getNode()->getVisualizationType() > 0 ) {
        saveToVisualization( _block, getSchain()->getNode()->getVisualizationType() );
    }

    try {
        CHECK_STATE( getLastCommittedBlockID() + 1 == _block->getBlockID() )

        printBlockLog( _block );

        proposalReceiptTime = 0;

        CHECK_STATE( _block->getBlockID() = getLastCommittedBlockID() + 1 )

        saveBlock( _block );

        cleanupUnneededMemoryBeforePushingToEvm( _block );

        auto evmProcessingStartMs = Time::getCurrentTimeMs();
        auto blockPushedToExtFaceTimeMs = evmProcessingStartMs;

        LOG( info,
            "CWT:" +
                to_string( blockPushedToExtFaceTimeMs -
                           pendingTransactionsAgent->transactionListReceivedTime() ) +
                ":TLWT:" + to_string( pendingTransactionsAgent->getTransactionListWaitTime() ) +
                ":SBPT:" + to_string( cryptoManager->sgxBlockProcessingTime() ) );
        pushBlockToExtFace( _block );
        auto evmProcessingTimeMs = Time::getCurrentTimeMs() - evmProcessingStartMs;

        auto stamp = TimeStamp( _block->getTimeStampS(), _block->getTimeStampMs() );

        updateLastCommittedBlockInfo( ( uint64_t ) _block->getBlockID(), stamp,
            _block->getTransactionList()->size(), evmProcessingTimeMs );

        // the last thing is to run analyzers to log any errors that happened during
        // block processing

        analyzeErrors( _block );

    } catch ( ExitRequestedException& e ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}

void Schain::saveBlock( const ptr< CommittedBlock >& _block ) {
    CHECK_ARGUMENT( _block );

    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    try {
        checkForExit();
        getNode()->getBlockDB()->saveBlock( _block );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}

void Schain::cleanupUnneededMemoryBeforePushingToEvm( const ptr< CommittedBlock > _block ) {
    CHECK_ARGUMENT( _block );

    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    try {
        getNode()->getBlockProposalDB()->cleanupUnneededMemoryBeforePushingToEvm( _block );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


void Schain::pushBlockToExtFace( const ptr< CommittedBlock >& _block ) {
    CHECK_ARGUMENT( _block );

    MONITOR2( __CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime() )

    checkForExit();

    try {
        auto tv = _block->getTransactionList()->createTransactionVector();

        // auto next_price = // VERIFY PRICING

        this->pricingAgent->calculatePrice(
            *tv, _block->getTimeStampS(), _block->getTimeStampMs(), _block->getBlockID() );

        auto currentPrice = this->pricingAgent->readPrice( _block->getBlockID() - 1 );

        // block boundary is the safesf place for exit
        // exit immediately if exit has been requested
        // this will initiate immediate exit and throw ExitRequestedException
        getSchain()->getNode()->checkForExitOnBlockBoundaryAndExitIfNeeded();

        if ( extFace ) {
            try {
                inCreateBlock = true;
                extFace->createBlock( *tv, _block->getTimeStampS(), _block->getTimeStampMs(),
                    ( __uint64_t ) _block->getBlockID(), currentPrice, _block->getStateRoot(),
                    ( uint64_t ) _block->getProposerIndex() );
                inCreateBlock = false;
            } catch ( ... ) {
                inCreateBlock = false;
                throw;
            }
        }

        // block boundary is the safesf place for exit
        // exit immediately if exit has been requested
        // this will initiate immediate exit and throw ExitRequestedException
        getSchain()->getNode()->checkForExitOnBlockBoundaryAndExitIfNeeded();

    } catch ( ExitRequestedException& e ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


void Schain::startConsensus(
    const block_id _blockID, const ptr< BooleanProposalVector >& _proposalVector ) {
    {
        proposalReceiptTime = Time::getCurrentTimeMs() - this->lastCommitTimeMs;
        CHECK_ARGUMENT( _proposalVector );

        MONITOR( __CLASS_NAME__, __FUNCTION__ )

        checkForExit();

        LOG( info, "CONSENSUS_STARTED:PROPOSING: " + _proposalVector->toString() );

        LOG( debug, "Got proposed block set for block:" + to_string( _blockID ) );

        LOG( debug, "StartConsensusIfNeeded BLOCK NUMBER:" + to_string( ( _blockID ) ) );

        if ( _blockID <= getLastCommittedBlockID() ) {
            LOG( debug, "Too late to start consensus: already committed " +
                            to_string( lastCommittedBlockID ) );
            return;
        }

        if ( _blockID > getLastCommittedBlockID() + 1 ) {
            LOG( debug, "Consensus is in the future" + to_string( lastCommittedBlockID ) );
            return;
        }
    }


    CHECK_STATE( blockConsensusInstance );
    CHECK_STATE( _proposalVector );

    auto message = make_shared< ConsensusProposalMessage >( *this, _blockID, _proposalVector );

    auto envelope = make_shared< InternalMessageEnvelope >( ORIGIN_EXTERNAL, message, *this );

    LOG( debug, "Starting consensus for block id:" + to_string( _blockID ) );
    postMessage( envelope );
}


void Schain::blockProposalReceiptTimeoutArrived( block_id _blockID ) {
    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    try {
        if ( _blockID <= getLastCommittedBlockID() )
            return;

        auto pv = getNode()->getDaProofDB()->getCurrentProposalVector( _blockID );

        CHECK_STATE( pv );

        // try starting consensus. It may already have been started due to
        // block proposals received
        tryStartingConsensus( pv, _blockID );
    } catch ( ExitRequestedException& e ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


void Schain::daProofArrived( const ptr< DAProof >& _daProof ) {
    CHECK_ARGUMENT( _daProof );

    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    try {
        if ( _daProof->getBlockId() <= getLastCommittedBlockID() )
            return;

        auto pv = getNode()->getDaProofDB()->addDAProof( _daProof );


        if ( pv != nullptr ) {
            auto bid = _daProof->getBlockId();

            // try starting consensus. It may already have been started due to
            // block proposal receipt timeout
            tryStartingConsensus( pv, bid );
        }
    } catch ( ExitRequestedException& e ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}

// Consensus is started after 2/3 N + 1 proposals are received, or BlockProposalTimeout is
// reached
void Schain::tryStartingConsensus( const ptr< BooleanProposalVector >& pv, const block_id& bid ) {
    auto needToStartConsensus =
        getNode()->getProposalVectorDB()->trySavingProposalVector( bid, pv );
    if ( needToStartConsensus )
        startConsensus( bid, pv );
}


void Schain::proposedBlockArrived( const ptr< BlockProposal >& _proposal ) {
    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    if ( _proposal->getBlockID() <= getLastCommittedBlockID() )
        return;

    CHECK_STATE( _proposal->getSignature() != "" );

    getNode()->getBlockProposalDB()->addBlockProposal( _proposal );
}


block_id Schain::readLastCommittedBlockIDFromDb() {
    return getNode()->getBlockDB()->readLastCommittedBlockID();
}

void Schain::updateInternalChainInfo( block_id _lastCommittedBlockID ) {
    getNode()->getInternalInfoDB()->updateInternalChainInfo( _lastCommittedBlockID );
}


void Schain::bootstrap( block_id _lastCommittedBlockID, uint64_t _lastCommittedBlockTimeStamp,
    uint64_t _lastCommittedBlockTimeStampMs ) {
    // should be called only once
    CHECK_STATE( !bootStrapped.exchange( true ) );

    updateInternalChainInfo( _lastCommittedBlockID );


    LOG( info, "Bootstrapping consensus ..." );

    auto lastCommittedBlockIDInConsensus = readLastCommittedBlockIDFromDb();

    LOG(
        info, "Last committed block in consensus:" + to_string( lastCommittedBlockIDInConsensus ) );

    LOG( info, "Last committed block in skaled:" + to_string( _lastCommittedBlockID ) );


    LOG( info, "Check the consensus database for corruption ..." );
    fixCorruptStateIfNeeded( lastCommittedBlockIDInConsensus );

    checkForExit();


    // catch situations that should never happen


    if ( lastCommittedBlockIDInConsensus > _lastCommittedBlockID + 128 ) {
        LOG( critical,
            "CRITICAL ERROR: consensus has way more blocks than skaled. This should never "
            "happen,"
            "since consensus passes blocks to skaled." );
        BOOST_THROW_EXCEPTION( InvalidStateException(
            "_lastCommittedBlockIDInConsensus > _lastCommittedBlockID + 128", __CLASS_NAME__ ) );
    }


    if ( lastCommittedBlockIDInConsensus < _lastCommittedBlockID ) {
        LOG( critical,
            "CRITICAL ERROR: last committed block in consensus is smaller than"
            " last committed block in skaled. This can never happen because consensus passes "
            "blocks to skaled" );

        BOOST_THROW_EXCEPTION( InvalidStateException(
            "_lastCommittedBlockIDInConsensus < lastCommittedBlockID in EVM", __CLASS_NAME__ ) );
    }


    // Step 0 Workaround for the fact that skaled does not yet save timestampMs

    if ( _lastCommittedBlockTimeStampMs == 0 && _lastCommittedBlockID > 0 ) {
        auto block = getNode()->getBlockDB()->getBlock( _lastCommittedBlockID, getCryptoManager() );
        if ( block ) {
            _lastCommittedBlockTimeStampMs = block->getTimeStampMs();
        };
    }


    // Step 1: solve block id  mismatch. Consensus may have more blocks than skaled
    // this can happen in case skaled crashed , can also happen when starting from a snapshot

    if ( lastCommittedBlockIDInConsensus > _lastCommittedBlockID ) {
        // consensus has several more blocks than skaled
        // This happens when starting from a snapshot
        // Since the snapshot is taken just before a block is processed
        // or after multiple skaled crashes
        // process these blocks


        LOG( warn,
            "Consensus has more blocks than skaled. This should not happen normally since "
            "consensus passes"
            "blocks to skaled.  Skaled may have crashed in the past." );

        while ( lastCommittedBlockIDInConsensus > _lastCommittedBlockID )

            try {
                auto block = getNode()->getBlockDB()->getBlock(
                    _lastCommittedBlockID + 1, getCryptoManager() );
                CHECK_STATE2( block, "No block in consensus, repair needed" );
                pushBlockToExtFace( block );
                _lastCommittedBlockID = _lastCommittedBlockID + 1;
                _lastCommittedBlockTimeStamp = block->getTimeStampS();
                _lastCommittedBlockTimeStampMs = block->getTimeStampMs();
                LOG( info, "Pushed block to skaled:" + _lastCommittedBlockID );
            } catch ( ... ) {
                // Cant read the block from db, may be it is corrupt in the  snapshot
                LOG( err, "Bootstrap could not read block from db. Repair." );
                // The block will be hopefully pulled by catchup
            }
    }

    MONITOR2( __CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime() )

    // Step 2 : now bootstrap

    LOG( info, "Starting normal boostrap ..." );

    try {
        bootstrapBlockID = ( uint64_t ) _lastCommittedBlockID;
        CHECK_STATE( _lastCommittedBlockTimeStamp < ( uint64_t ) 2 * MODERN_TIME );

        TimeStamp stamp( _lastCommittedBlockTimeStamp, _lastCommittedBlockTimeStampMs );
        initLastCommittedBlockInfo( ( uint64_t ) _lastCommittedBlockID, stamp );


        LOG( info, "Jump starting the system with block:" + to_string( _lastCommittedBlockID ) );

        if ( getLastCommittedBlockID() == 0 )
            this->pricingAgent->calculatePrice( ConsensusExtFace::transactions_vector(), 0, 0, 0 );

        isStateInitialized = true;


        if ( getNode()->isSyncOnlyNode() )
            return;

        {
            lock_guard< timed_mutex > lock( ( blockProcessMutex ) );
            auto emptyBlockInterval = getNode()->getEmptyBlockIntervalMs();
            // do not wait much for the first block after start
            // otherwise bootStrapAll() can block node start
            getNode()->setEmptyBlockIntervalMs( 50 );
            proposeNextBlock();
            getNode()->setEmptyBlockIntervalMs( emptyBlockInterval );
            LOG( info, "Successfully proposed block in boostrap" );
        }


        ifIncompleteConsensusDetectedRestartAndRebroadcastAllMessagesForCurrentBlock();
        LOG( info, "Successfully completed boostrap" );
    } catch ( exception& e ) {
        SkaleException::logNested( e );
        return;
    }
}

void Schain::ifIncompleteConsensusDetectedRestartAndRebroadcastAllMessagesForCurrentBlock() {
    auto proposalVector = getNode()->getProposalVectorDB()->getVector( lastCommittedBlockID + 1 );
    if ( proposalVector ) {
        startConsensus( lastCommittedBlockID + 1, proposalVector );
        LOG( info, "Incompleted consensus detected." );

        auto messages = getNode()->getOutgoingMsgDB()->getMessages( lastCommittedBlockID + 1 );
        CHECK_STATE( messages );
        LOG( info, "Rebroadcasting " + to_string( messages->size() ) + " messages for block " +
                       to_string( lastCommittedBlockID + 1 ) );
        for ( auto&& m : *messages ) {
            getNode()->getNetwork()->rebroadcastMessage( m );
        }
    }
}

void Schain::rebroadcastAllMessagesForCurrentBlock() {
    auto messages = getNode()->getOutgoingMsgDB()->getMessages( lastCommittedBlockID + 1 );
    CHECK_STATE( messages );
    LOG( info, "Rebroadcasting " + to_string( messages->size() ) + " messages for block " +
                   to_string( lastCommittedBlockID + 1 ) );
    for ( auto&& m : *messages ) {
        getNode()->getNetwork()->rebroadcastMessage( m );
    }
}


void Schain::healthCheck() {
    std::unordered_set< uint64_t > connections;
    setHealthCheckFile( 1 );

    auto beginTime = Time::getCurrentTimeSec();
    auto lastWarningPrintTimeSec = 0;

    LOG( info, "Waiting to connect to peers (could be up to two minutes)" );


    while ( connections.size() + 1 < getNodeCount() ) {
        // will optimistically wait for all nodes.
        // if not all nodes are present, will be satisfied by 2/3 nodes

        if ( 3 * ( connections.size() + 1 ) >= 2 * getNodeCount() ) {
            if ( Time::getCurrentTimeSec() - beginTime >
                 HEALTH_CHECK_TIME_TO_WAIT_FOR_ALL_NODES_SEC ) {
                break;
            }
        }

        // If the health check has been runnning for a long time and one could not connect to
        // 2/3 nodes skaled will restart
        if ( Time::getCurrentTimeSec() - beginTime > HEALTHCHECK_ON_START_RETRY_TIME_SEC ) {
            setHealthCheckFile( 0 );
            LOG( err, "Coult not connect to 2/3 of peers" );
            exit( 110 );
        }

        // check if it is time to print a warning again and print it
        if ( Time::getCurrentTimeSec() - lastWarningPrintTimeSec >
             HEALTHCHECK_ON_START_TIME_BETWEEN_WARNINGS_SEC ) {
            LOG( warn, "Coult not connect to 2/3 of peers. Retrying ..." );
            string aliveNodeIndices = "Alive node indices:";

            for ( auto& index : connections ) {
                aliveNodeIndices += to_string( index ) + ":";
            };

            LOG( warn, aliveNodeIndices );

            lastWarningPrintTimeSec = Time::getCurrentTimeSec();
        }


        if ( getNode()->isExitRequested() ) {
            BOOST_THROW_EXCEPTION( ExitRequestedException( __CLASS_NAME__ ) );
        }

        usleep( TIME_BETWEEN_STARTUP_HEALTHCHECK_RETRIES_SEC * 1000000 );

        for ( int i = 1; i <= getNodeCount(); i++ ) {
            if ( i != ( getSchainIndex() ) && !connections.count( i ) ) {
                try {
                    if ( getNode()->isExitRequested() ) {
                        BOOST_THROW_EXCEPTION( ExitRequestedException( __CLASS_NAME__ ) );
                    }

                    auto port =
                        ( getNode()->isSyncOnlyNode() ? port_type::CATCHUP : port_type::PROPOSAL );

                    auto socket = make_shared< ClientSocket >( *this, schain_index( i ), port );
                    LOG( debug, "Health check: connected to peer" );
                    getIo()->writeMagic( socket, true );
                    connections.insert( i );
                } catch ( ExitRequestedException& ) {
                    throw;
                } catch ( std::exception& e ) {
                }
            }
        }
    }

    LOG( info, "Successfully connected to two thirds of peers" );

    setHealthCheckFile( 2 );
}

void Schain::daProofSigShareArrived(
    const ptr< ThresholdSigShare >& _sigShare, const ptr< BlockProposal >& _proposal ) {
    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    checkForExit();

    CHECK_ARGUMENT( _sigShare != nullptr );
    CHECK_ARGUMENT( _proposal != nullptr );


    try {
        auto proof =
            getNode()->getDaSigShareDB()->addAndMergeSigShareAndVerifySig( _sigShare, _proposal );
        if ( proof != nullptr ) {
            getSchain()->daProofArrived( proof );
            blockProposalClient->enqueueItem( proof );
        }
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        LOG( err, "Could not add/merge sig" );
        throw_with_nested( InvalidStateException( "Could not add/merge sig", __CLASS_NAME__ ) );
    }
}


void Schain::constructServers( const ptr< Sockets >& _sockets ) {
    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    catchupServerAgent = make_shared< CatchupServerAgent >( *this, _sockets->catchupSocket );


    if ( getNode()->isSyncOnlyNode() )
        return;

    blockProposalServerAgent =
        make_shared< BlockProposalServerAgent >( *this, _sockets->blockProposalSocket );
}

ptr< BlockProposal > Schain::createDefaultEmptyBlockProposal( block_id _blockId ) {
    TimeStamp newStamp;

    {
        lock_guard< mutex > l( lastCommittedBlockInfoMutex );
        newStamp = lastCommittedBlockTimeStamp.incrementByMs();
    }

    return make_shared< ReceivedBlockProposal >(
        *this, _blockId, newStamp.getS(), newStamp.getMs(), 0 );
}


void Schain::finalizeDecidedAndSignedBlock( block_id _blockId, schain_index _proposerIndex,
    const ptr< ThresholdSignature >& _thresholdSig ) {
    CHECK_ARGUMENT( _thresholdSig != nullptr );


    MONITOR2( __CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime() )


    if ( _blockId <= getLastCommittedBlockID() ) {
        LOG( debug, "Ignoring old block decide, already got this through catchup: BID:" +
                        to_string( _blockId ) + ":PRP:" + to_string( _proposerIndex ) );
        return;
    }


    try {
        if ( _proposerIndex == 0 ) {
            // default empty block
            blockCommitArrived( _blockId, _proposerIndex, _thresholdSig, nullptr );
            return;
        }

        ptr< BlockProposal > proposal = nullptr;
        ptr< ThresholdSignature > daSig;

        proposal = getNode()->getBlockProposalDB()->getBlockProposal( _blockId, _proposerIndex );


        // Figure out if we need to download proposal

        bool downloadProposal;

        if ( proposal ) {
            auto daProofSig = getNode()->getDaProofDB()->getDASig( _blockId, _proposerIndex );
            // a proposal without a  DA proof is not trusted and has to be
            downloadProposal = daProofSig.empty();
            if ( !downloadProposal ) {
                auto hash = proposal->getHash();
                daSig = getSchain()->getCryptoManager()->verifyDAProofThresholdSig(
                    hash, daProofSig, _blockId, proposal->getTimeStampS() );
            }
        } else {
            downloadProposal = true;
        }

        if ( downloadProposal ||
             // downloaded from others this switch is for testing only
             getNode()->getTestConfig()->isFinalizationDownloadOnly() ) {
            // did not receive proposal from the proposer, pull it in parallel from other hosts
            // Note that due to the BLS signature proof, 2t hosts out of 3t + 1 total are
            // guaranteed to posess the proposal

            LOG( info, "FINALIZING_BLOCK:BID:" + to_string( _blockId ) );

            auto agent = make_unique< BlockFinalizeDownloader >( this, _blockId, _proposerIndex );

            {
                const string msg = "Finalization download:" + to_string( _blockId ) + ":" +
                                   to_string( _proposerIndex );

                MONITOR( __CLASS_NAME__, msg.c_str() );
                // This will complete successfully also if block arrives through catchup
                proposal = agent->downloadProposal();
                // if null is returned it means that catchup happened first and
                // the block will be processed through catchup
                if ( proposal )
                    daSig = agent->getDaSig( proposal->getTimeStampS() );
            }

            if ( proposal )  // Nullptr means catchup happened first
                getNode()->getBlockProposalDB()->addBlockProposal( proposal );
        }

        if ( proposal ) {
            blockCommitArrived( _blockId, _proposerIndex, _thresholdSig, daSig );
        }

    } catch ( ExitRequestedException& e ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}

// empty constructor is used for tests
Schain::Schain() : Agent() {}

bool Schain::fixCorruptStateIfNeeded( block_id _lastCommittedBlockID ) {
    block_id nextBlock = _lastCommittedBlockID + 1;
    if ( getNode()->getBlockDB()->unfinishedBlockExists( nextBlock ) ) {
        return true;
        LOG( warn,
            "Corrupt consensus database has been repaired successfully."
            "Starting from repaired consensus database." );
    } else {
        return false;
    }
}


void Schain::startStatusServer() {
    if ( !s ) {
        httpserver = make_shared< jsonrpc::HttpServer >(
            ( int ) ( ( uint16_t ) getNode()->getBasePort() + STATUS ), "", "", "", 1 );
        s = make_shared< StatusServer >( this, *httpserver, jsonrpc::JSONRPC_SERVER_V1V2 );
    }

#ifdef CONSENSUS_DEMO
    CHECK_STATE( s );
    LOG( info, "Starting status server ..." );
    CHECK_STATE( s->StartListening() );
    LOG( info, "Successfully started status server ..." );
#endif
}

void Schain::stopStatusServer() {
    if ( s )
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

void Schain::addDeadNode( uint64_t _schainIndex, uint64_t _checkTime ) {
    CHECK_STATE( _schainIndex > 0 );
    CHECK_STATE( _schainIndex <= getNodeCount() );
    {
        lock_guard< mutex > l( deadNodesLock );
        if ( deadNodes.count( _schainIndex ) == 0 ) {
            deadNodes.insert( { _schainIndex, _checkTime } );
        }
    }
}

void Schain::markAliveNode( uint64_t _schainIndex ) {
    CHECK_STATE( _schainIndex > 0 );
    CHECK_STATE( _schainIndex <= getNodeCount() );
    {
        lock_guard< mutex > l( deadNodesLock );
        if ( deadNodes.count( _schainIndex ) > 0 ) {
            deadNodes.erase( _schainIndex );
        }
    }
}

uint64_t Schain::getDeathTimeMs( uint64_t _schainIndex ) {
    CHECK_STATE( _schainIndex > 0 );
    CHECK_STATE( _schainIndex <= getNodeCount() );
    {
        lock_guard< mutex > l( deadNodesLock );
        if ( deadNodes.count( _schainIndex ) == 0 ) {
            return 0;
        } else {
            return deadNodes.at( _schainIndex );
        }
    }
}

ptr< ofstream > Schain::getVisualizationDataStream() {
    lock_guard< mutex > l( vdsMutex );
    if ( !visualizationDataStream ) {
        visualizationDataStream = make_shared< ofstream >();
        visualizationDataStream->exceptions( std::ofstream::badbit | std::ofstream::failbit );
        auto t = Time::getCurrentTimeMs();
        auto fileName = "/tmp/consensusv_" + to_string( t ) + ".data";
        visualizationDataStream->open( fileName, ios_base::trunc );
    }
    return visualizationDataStream;
}

void Schain::writeToVisualizationStream( string& _s ) {
    lock_guard< mutex > l( vdsMutex );
    auto stream = getVisualizationDataStream();
    stream->write( _s.c_str(), _s.size() );
}


u256 Schain::getRandomForBlockId( block_id _blockId ) {
    auto block = getBlock( _blockId );
    CHECK_STATE( block );
    auto signature = block->getThresholdSig();

    auto data = make_shared< vector< uint8_t > >();

    for ( uint64_t i = 0; i < signature.size(); i++ ) {
        data->push_back( ( uint8_t ) signature.at( i ) );
    }

    auto hash = BLAKE3Hash::calculateHash( data );
    return u256( "0x" + hash.toHex() );
}

ptr< ofstream > Schain::visualizationDataStream = nullptr;

const ptr< OracleResultAssemblyAgent >& Schain::getOracleResultAssemblyAgent() const {
    return oracleResultAssemblyAgent;
}

void Schain::addBlockErrorAnalyzer( ptr< BlockErrorAnalyzer > _blockErrorAnalyzer ) {
    {
        LOCK( blockErrorAnalyzersMutex )
        blockErrorAnalyzers.push_back( _blockErrorAnalyzer );
    }
}


void Schain::analyzeErrors( ptr< CommittedBlock > _block ) {
    vector< ptr< BlockErrorAnalyzer > > analyzers;

    {
        LOCK( blockErrorAnalyzersMutex )
        analyzers = blockErrorAnalyzers;
        blockErrorAnalyzers = vector< ptr< BlockErrorAnalyzer > >();
    }

    for ( auto&& analyzer : analyzers ) {
        analyzer->analyze( _block );
    }
}
uint64_t Schain::getVerifyDaSigsPatchTimestampS() const {
    return verifyDaSigsPatchTimestampS;
}


mutex Schain::vdsMutex;
