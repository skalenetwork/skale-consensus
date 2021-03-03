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


#include <sched.h>
#include "leveldb/db.h"
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
#include "db/DAProofDB.h"
#include "db/DASigShareDB.h"
#include "db/ProposalVectorDB.h"
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
#include "network/ClientSocket.h"
#include "network/IO.h"
#include "network/Sockets.h"
#include "network/ZMQSockets.h"
#include "node/NodeInfo.h"
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

void Schain::postMessage( const ptr< MessageEnvelope >& _me ) {
    CHECK_ARGUMENT( _me );

    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    checkForExit();

    CHECK_STATE( ( uint64_t ) _me->getMessage()->getBlockId() != 0 );
    {
        lock_guard< mutex > lock( messageMutex );
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
                    SkaleException::logNested( e );

                    if ( _sChain->getNode()->isExitRequested() )
                        return;
                }  // catch

                newQueue.pop();
            }
        }


        _sChain->getNode()->getSockets()->consensusZMQSockets->closeSend();
    } catch ( FatalError& e ) {
        SkaleException::logNested( e );
        _sChain->getNode()->exitOnFatalError( e.what() );
    }
}


void Schain::startThreads() {
    CHECK_STATE( consensusMessageThreadPool );
    this->consensusMessageThreadPool->startService();
}


Schain::Schain( weak_ptr< Node > _node, schain_index _schainIndex, const schain_id& _schainID,
    ConsensusExtFace* _extFace )
    : Agent( *this, true, true ),
      totalTransactions( 0 ),
      extFace( _extFace ),
      schainID( _schainID ),
      startTimeMs( 0 ),
      consensusMessageThreadPool( new SchainMessageThreadPool( this ) ),
      node( _node ),
      schainIndex( _schainIndex ) {
    lastCommittedBlockTimeStamp = make_shared< TimeStamp >( 0, 0 );

    // construct monitoring and timeout agents early
    monitoringAgent = make_shared< MonitoringAgent >( *this );
    timeoutAgent = make_shared< TimeoutAgent >( *this );


    maxExternalBlockProcessingTime =
        std::max( 2 * getNode()->getEmptyBlockIntervalMs(), ( uint64_t ) 3000 );

    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    CHECK_STATE( schainIndex > 0 );

    try {
        this->io = make_shared< IO >( this );

        CHECK_STATE( getNode()->getNodeInfosByIndex()->size() > 0 );

        for ( auto const& iterator : *getNode()->getNodeInfosByIndex() ) {
            if ( iterator.second->getNodeID() == getNode()->getNodeID() ) {
                CHECK_STATE( thisNodeInfo == nullptr && iterator.second != nullptr );
                thisNodeInfo = iterator.second;
            }
        }

        if ( thisNodeInfo == nullptr ) {
            BOOST_THROW_EXCEPTION( EngineInitException(
                "Schain: " + to_string( ( uint64_t ) getSchainID() ) +
                    " does not include current node with IP " + getNode()->getBindIP() +
                    "and node id " + to_string( getNode()->getNodeID() ),
                __CLASS_NAME__ ) );
        }

        CHECK_STATE( getNodeCount() > 0 );

        constructChildAgents();

        string none = SchainTest::NONE;

        blockProposerTest = none;

        getNode()->registerAgent( this );

    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( FatalError( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


void Schain::constructChildAgents() {
    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    try {
        LOCK( m )
        pendingTransactionsAgent = make_shared< PendingTransactionsAgent >( *this );
        blockProposalClient = make_shared< BlockProposalClientAgent >( *this );
        catchupClientAgent = make_shared< CatchupClientAgent >( *this );


        testMessageGeneratorAgent = make_shared< TestMessageGeneratorAgent >( *this );
        pricingAgent = make_shared< PricingAgent >( *this );
        cryptoManager = make_shared< CryptoManager >( *this );

    } catch ( ... ) {
        throw_with_nested( FatalError( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


void Schain::blockCommitsArrivedThroughCatchup( const ptr< CommittedBlockList >& _blockList ) {
    CHECK_ARGUMENT( _blockList );

    auto blocks = _blockList->getBlocks();

    CHECK_STATE( blocks );

    if ( blocks->size() == 0 ) {
        return;
    }

    LOCK( m )

    bumpPriority();

    atomic< uint64_t > committedIDOld = ( uint64_t ) getLastCommittedBlockID();

    CHECK_STATE( blocks->at( 0 )->getBlockID() <= ( uint64_t ) getLastCommittedBlockID() + 1 );

    for ( size_t i = 0; i < blocks->size(); i++ ) {
        auto block = blocks->at( i );

        CHECK_STATE( block );

        if ( ( uint64_t ) block->getBlockID() == (getLastCommittedBlockID()  + 1)) {
            CHECK_STATE(*getLastCommittedBlockTimeStamp() < *block->getTimeStamp());
            processCommittedBlock( block );
        }
    }

    if ( committedIDOld < getLastCommittedBlockID() ) {
        LOG( info, "BLOCK_CATCHUP: " + to_string( getLastCommittedBlockID() - committedIDOld ) +
                       " BLOCKS" );
        proposeNextBlock();
    }

    unbumpPriority();
}


void Schain::blockCommitArrived( block_id _committedBlockID, schain_index _proposerIndex,
    const ptr< ThresholdSignature >& _thresholdSig ) {
    MONITOR2( __CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime() )

    checkForExit();

    LOCK( m )
    
    if ( _committedBlockID <= getLastCommittedBlockID() )
        return;



    CHECK_STATE(
        _committedBlockID == ( getLastCommittedBlockID() + 1 ) || getLastCommittedBlockID() == 0 );

    bumpPriority();

    try {
        ptr< BlockProposal > committedProposal = nullptr;

        if ( _proposerIndex > 0 ) {
            committedProposal = getNode()->getBlockProposalDB()->getBlockProposal(
                _committedBlockID, _proposerIndex );
        } else {
            committedProposal = createDefaultEmptyBlockProposal( _committedBlockID );
        }

        CHECK_STATE( committedProposal );

        auto newCommittedBlock = CommittedBlock::makeObject( committedProposal, _thresholdSig );

        CHECK_STATE(*getLastCommittedBlockTimeStamp() < *newCommittedBlock->getTimeStamp());


        processCommittedBlock( newCommittedBlock );
        proposeNextBlock();

        unbumpPriority();

    } catch ( ExitRequestedException& e ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


void Schain::checkForExit() {
    if ( getNode()->isExitRequested() ) {
        BOOST_THROW_EXCEPTION( ExitRequestedException( __CLASS_NAME__ ) );
    }
}

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
            myProposal = pendingTransactionsAgent->buildBlockProposal(
                _proposedBlockID, lastCommittedBlockTimeStamp );
        }


        CHECK_STATE( myProposal );

        CHECK_STATE( myProposal->getProposerIndex() == getSchainIndex() );
        CHECK_STATE( myProposal->getSignature() != "" );


        proposedBlockArrived( myProposal );

        LOG( debug, "PROPOSING BLOCK NUMBER:" + to_string( _proposedBlockID ) );

        auto db = getNode()->getProposalHashDB();

        db->checkAndSaveHash( _proposedBlockID, getSchainIndex(), myProposal->getHash()->toHex() );

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
    params.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(this_thread, SCHED_FIFO, &params);
}

void Schain::unbumpPriority() {
    struct sched_param params;
    // Set the priority to norm
    pthread_t this_thread = pthread_self();
    params.sched_priority = 0;
    CHECK_STATE(pthread_setschedparam(this_thread, 0, &params) == 0)
}

void Schain::processCommittedBlock( const ptr< CommittedBlock >& _block ) {
    CHECK_ARGUMENT( _block );
    MONITOR2( __CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime() )

    checkForExit();

    LOCK( m )

    try {
        CHECK_STATE( getLastCommittedBlockID() + 1 == _block->getBlockID() )

        totalTransactions += _block->getTransactionList()->size();

        auto h = _block->getHash()->toHex().substr( 0, 8 );

        auto stamp = make_shared< TimeStamp >( _block->getTimeStampS(), _block->getTimeStampMs() );

        LOG( info,
            "BLOCK_COMMIT: PRPSR:" + to_string( _block->getProposerIndex() ) +
                ":BID: " + to_string( _block->getBlockID() ) +
                ":ROOT:" + _block->getStateRoot().convert_to< string >() + ":HASH:" + h +
                ":BLOCK_TXS:" + to_string( _block->getTransactionCount() ) +
                ":DMSG:" + to_string( getMessagesCount() ) +
                ":MPRPS:" + to_string( MyBlockProposal::getTotalObjects() ) +
                ":RPRPS:" + to_string( ReceivedBlockProposal::getTotalObjects() ) +
                ":TXS:" + to_string( Transaction::getTotalObjects() ) +
                ":TXLS:" + to_string( TransactionList::getTotalObjects() ) +
                ":KNWN:" + to_string( pendingTransactionsAgent->getKnownTransactionsSize() ) +
                ":MGS:" + to_string( Message::getTotalObjects() ) +
                ":INSTS:" + to_string( ProtocolInstance::getTotalObjects() ) +
                ":BPS:" + to_string( BlockProposalSet::getTotalObjects() ) +
                ":HDRS:" + to_string( Header::getTotalObjects() ) +
                ":SOCK:" + to_string( ClientSocket::getTotalSockets() ) +
                ":CONS:" + to_string( ServerConnection::getTotalObjects() ) + ":DSDS:" +
                to_string( getSchain()->getNode()->getNetwork()->computeTotalDelayedSends() ) +
                ":FDS:" + to_string(ConsensusEngine::getOpenDescriptors()) +
                ":PRT:" + to_string(proposalReceiptTime) +
                ":STAMP:" + stamp->toString() );

        proposalReceiptTime = 0;

        CHECK_STATE(_block->getBlockID() = getLastCommittedBlockID() + 1)

        pushBlockToExtFace( _block );

        saveBlock( _block );

        updateLastCommittedBlockInfo(
            ( uint64_t ) _block->getBlockID(), stamp, _block->getTransactionList()->size() );

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


        if ( extFace ) {

            extFace->createBlock( *tv, _block->getTimeStampS(), _block->getTimeStampMs(),
                ( __uint64_t ) _block->getBlockID(), currentPrice, _block->getStateRoot(),
                ( uint64_t ) _block->getProposerIndex() );
            // exit immediately if exit has been requested
            getSchain()->getNode()->exitCheck();
        }

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

        LOG( info, "BIN_CONSENSUS_START: PROPOSING: " + _proposalVector->toString() );

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

// Consensus is started after 2/3 N + 1 proposals are received, or BlockProposalTimeout is reached
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


void Schain::bootstrap( block_id _lastCommittedBlockID, uint64_t _lastCommittedBlockTimeStamp,
    uint64_t _lastCommittedBlockTimeStampMs ) {
    LOG( info, "Bootstrapping consensus ..." );
    auto _lastCommittedBlockIDInConsensus = getNode()->getBlockDB()->readLastCommittedBlockID();
    LOG( info, "Check the consensus database for corruption ..." );
    this->startingFromCorruptState = fixCorruptStateIfNeeded( _lastCommittedBlockIDInConsensus );
    if ( startingFromCorruptState ) {
        LOG( warn,
            "Corrupt consensus database has been repaired successfully."
            "Starting from repaired consensus database." );
    }

    LOG( info,
        "Last committed block in consensus:" + to_string( _lastCommittedBlockIDInConsensus ) );

    checkForExit();

    // Step 0 Workaround for the fact that skaled does not yet save timestampMs

    if (_lastCommittedBlockTimeStampMs == 0 && _lastCommittedBlockID > 0) {
        auto block = getNode()->getBlockDB()->getBlock(
            _lastCommittedBlockID, getCryptoManager() );
        if (block) {
            _lastCommittedBlockTimeStampMs = block->getTimeStampMs();
        };
    }

    // Step 1: solve block id  mismatch


    if ( _lastCommittedBlockIDInConsensus == _lastCommittedBlockID + 1 ) {
        // consensus has one more block than skaled
        // This happens when starting from a snapshot
        // Since the snapshot is taken just before a block is processed
        try {
            auto block = getNode()->getBlockDB()->getBlock(
                _lastCommittedBlockIDInConsensus, getCryptoManager() );
            if ( block != nullptr ) {
                // we have one more block in consensus, so we push it out

                pushBlockToExtFace( block );
                _lastCommittedBlockID = _lastCommittedBlockID + 1;
            }
        } catch ( ... ) {
            // Cant read the block form db, may be it is corrupt in the  snapshot
            LOG( err, "Bootstrap could not read block from db" );
            // The block will be pulled by catchup
        }
    } else {
        // catch situations that should never happen
        if ( _lastCommittedBlockIDInConsensus < _lastCommittedBlockID ) {
            BOOST_THROW_EXCEPTION( InvalidStateException(
                "_lastCommittedBlockIDInConsensus < _lastCommittedBlockID", __CLASS_NAME__ ) );
        }

        if ( _lastCommittedBlockIDInConsensus > _lastCommittedBlockID + 1 ) {
            BOOST_THROW_EXCEPTION( InvalidStateException(
                "_lastCommittedBlockIDInConsensus >& _lastCommittedBlockID + 1", __CLASS_NAME__ ) );
        }
    }

    MONITOR2( __CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime() )

    // Step 2 : Bootstrap

    try {
        CHECK_STATE( !bootStrapped );
        bootStrapped = true;
        bootstrapBlockID.store( ( uint64_t ) _lastCommittedBlockID );
        CHECK_STATE( _lastCommittedBlockTimeStamp < ( uint64_t ) 2 * MODERN_TIME );

        LOCK( m )

        ptr< BlockProposal > committedProposal = nullptr;


         initLastCommittedBlockInfo( ( uint64_t ) _lastCommittedBlockID,
            make_shared< TimeStamp >(
                _lastCommittedBlockTimeStamp, _lastCommittedBlockTimeStampMs ) );


        LOG( info, "Jump starting the system with block:" + to_string( _lastCommittedBlockID ) );
        if ( getLastCommittedBlockID() == 0 )
            this->pricingAgent->calculatePrice( ConsensusExtFace::transactions_vector(), 0, 0, 0 );

        proposeNextBlock();

        rebroadcastAllMessagesForCurrentBlock();

    } catch ( exception& e ) {
        SkaleException::logNested( e );
        return;
    }
}
void Schain::rebroadcastAllMessagesForCurrentBlock() const {
    auto proposalVector = getNode()->getProposalVectorDB()->getVector( lastCommittedBlockID + 1 );
    if ( proposalVector ) {
        LOG( info, "Rebroadcasting messages for the current block" );
        auto messages = getNode()->getOutgoingMsgDB()->getMessages( lastCommittedBlockID + 1 );
        for ( auto&& m : *messages ) {
            getNode()->getNetwork()->rebroadcastMessage( m );
        }
    }
}


void Schain::healthCheck() {
    std::unordered_set< uint64_t > connections;
    setHealthCheckFile( 1 );

    auto beginTime = Time::getCurrentTimeSec();

    LOG( info, "Waiting to connect to peers" );


    while ( connections.size() + 1 < getNodeCount() ) {
        if ( 3 * ( connections.size() + 1 ) >= 2 * getNodeCount() ) {
            if ( Time::getCurrentTimeSec() - beginTime > 5 ) {
                break;
            }
        }

        if ( Time::getCurrentTimeSec() - beginTime > 15000 ) {
            setHealthCheckFile( 0 );
            LOG( err, "Coult not connect to 2/3 of peers" );
            exit( 110 );
        }

        if ( getNode()->isExitRequested() ) {
            BOOST_THROW_EXCEPTION( ExitRequestedException( __CLASS_NAME__ ) );
        }

        usleep( 1000000 );

        if ( getNode()->isExitRequested() ) {
            BOOST_THROW_EXCEPTION( ExitRequestedException( __CLASS_NAME__ ) );
        }

        for ( int i = 1; i <= getNodeCount(); i++ ) {
            if ( i != ( getSchainIndex() ) && !connections.count( i ) ) {
                try {
                    if ( getNode()->isExitRequested() ) {
                        BOOST_THROW_EXCEPTION( ExitRequestedException( __CLASS_NAME__ ) );
                    }
                    auto socket = make_shared< ClientSocket >(
                        *this, schain_index( i ), port_type::PROPOSAL );
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

    blockProposalServerAgent =
        make_shared< BlockProposalServerAgent >( *this, _sockets->blockProposalSocket );
    catchupServerAgent = make_shared< CatchupServerAgent >( *this, _sockets->catchupSocket );
}

ptr< BlockProposal > Schain::createDefaultEmptyBlockProposal( block_id _blockId ) {
    CHECK_STATE( lastCommittedBlockTimeStamp );

    auto newStamp = lastCommittedBlockTimeStamp->incrementByMs();

    return make_shared< ReceivedBlockProposal >(
        *this, _blockId, newStamp->getS(), newStamp->getMs(), 0 );
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


    LOG( info, "BLOCK_SIGNED: Now finalizing block ... BID:" + to_string( _blockId ) );


    try {
        if ( _proposerIndex == 0 ) {
            // default empty block
            blockCommitArrived( _blockId, _proposerIndex, _thresholdSig );
            return;
        }

        ptr< BlockProposal > proposal = nullptr;

        proposal = getNode()->getBlockProposalDB()->getBlockProposal( _blockId, _proposerIndex );


        // Figure out if we need to download proposal

        bool downloadProposal;

        if (proposal) {
            // a proposal without a  DA proof is not trusted and has to be
            downloadProposal = !getNode()->getDaProofDB()->haveDAProof( proposal );
        } else {
            downloadProposal = true;
        }

        if ( downloadProposal ||
                            // downloaded from others this switch is for testing only
             getNode()->getTestConfig()->isFinalizationDownloadOnly() ) {
            // did not receive proposal from the proposer, pull it in parallel from other hosts
            // Note that due to the BLS signature proof, 2t hosts out of 3t + 1 total are guaranteed
            // to posess the proposal

            auto agent = make_unique< BlockFinalizeDownloader >( this, _blockId, _proposerIndex );

            {
                const string msg = "Finalization download:" + to_string( _blockId ) + ":" +
                                   to_string( _proposerIndex );

                MONITOR( __CLASS_NAME__, msg.c_str() );
                // This will complete successfully also if block arrives through catchup
                proposal = agent->downloadProposal();
            }

            if (proposal)  // Nullptr means catchup happened first
                getNode()->getBlockProposalDB()->addBlockProposal( proposal );
        }

        if ( proposal) {
            blockCommitArrived( _blockId, _proposerIndex, _thresholdSig );
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
    } else {
        return false;
    }
}


bool Schain::isStartingFromCorruptState() const {
    return startingFromCorruptState;
}
