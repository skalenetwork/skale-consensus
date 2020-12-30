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
#include "crypto/ConsensusBLSSigShare.h"
#include "crypto/BLAKE3Hash.h"
#include "crypto/ThresholdSignature.h"
#include "datastructures/BlockProposal.h"
#include "datastructures/BlockProposalSet.h"
#include "datastructures/BooleanProposalVector.h"
#include "datastructures/CommittedBlock.h"
#include "datastructures/CommittedBlockList.h"
#include "datastructures/DAProof.h"
#include "datastructures/MyBlockProposal.h"
#include "datastructures/ReceivedBlockProposal.h"
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

void Schain::postMessage(const ptr< MessageEnvelope >& _me ) {
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
                    if ( _sChain->getNode()->isExitRequested() ) {
                        _sChain->getNode()->getSockets()->consensusZMQSockets->closeSend();
                        return;
                    }
                }

                newQueue = _sChain->messageQueue;

                while ( !_sChain->messageQueue.empty() ) {
                    if ( _sChain->getNode()->isExitRequested() ) {
                        _sChain->getNode()->getSockets()->consensusZMQSockets->closeSend();
                        return;
                    }
                    _sChain->messageQueue.pop();
                }
            }

            while ( !newQueue.empty() ) {
                if ( _sChain->getNode()->isExitRequested() ) {
                    _sChain->getNode()->getSockets()->consensusZMQSockets->closeSend();
                    return;
                }
                ptr< MessageEnvelope > m = newQueue.front();
                CHECK_STATE( ( uint64_t ) m->getMessage()->getBlockId() != 0 );

                try {
                    _sChain->getBlockConsensusInstance()->routeAndProcessMessage( m );
                } catch ( exception& e ) {
                    if ( _sChain->getNode()->isExitRequested() ) {
                        _sChain->getNode()->getSockets()->consensusZMQSockets->closeSend();
                        return;
                    }
                    SkaleException::logNested( e );
                }

                newQueue.pop();
            }
        }


        _sChain->getNode()->getSockets()->consensusZMQSockets->closeSend();
    } catch ( FatalError& e ) {
        _sChain->getNode()->exitOnFatalError( e.getMessage() );
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
      consensusMessageThreadPool( new SchainMessageThreadPool( this ) ),
      node( _node ),
      schainIndex( _schainIndex ) {
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


void Schain::blockCommitsArrivedThroughCatchup(const ptr< CommittedBlockList >& _blockList ) {
    CHECK_ARGUMENT( _blockList );

    auto blocks = _blockList->getBlocks();

    CHECK_STATE( blocks );

    if ( blocks->size() == 0 ) {
        return;
    }

    LOCK( m )

    atomic< uint64_t > committedIDOld = ( uint64_t ) getLastCommittedBlockID();

    uint64_t previosBlockTimeStamp = 0;
    uint64_t previosBlockTimeStampMs = 0;

    CHECK_STATE( blocks->at( 0 )->getBlockID() <= ( uint64_t ) getLastCommittedBlockID() + 1 );

    for ( size_t i = 0; i < blocks->size(); i++ ) {
        auto block = blocks->at( i );

        CHECK_STATE( block );

        if ( ( uint64_t ) block->getBlockID() > getLastCommittedBlockID() ) {
            processCommittedBlock( block );
            previosBlockTimeStamp = block->getTimeStamp();
            previosBlockTimeStampMs = block->getTimeStampMs();
        }
    }

    if ( committedIDOld < getLastCommittedBlockID() ) {
        LOG( info, "BLOCK_CATCHUP: " + to_string( getLastCommittedBlockID() - committedIDOld ) +
                       " BLOCKS" );
        proposeNextBlock( previosBlockTimeStamp, previosBlockTimeStampMs );
    }
}


void Schain::blockCommitArrived( block_id _committedBlockID, schain_index _proposerIndex,
    uint64_t _committedTimeStamp, uint64_t _committedTimeStampMs,
 const ptr< ThresholdSignature >& _thresholdSig ) {
    MONITOR2( __CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime() )

    checkForExit();

    CHECK_STATE( _committedTimeStamp < ( uint64_t ) 2 * MODERN_TIME );

    LOCK( m )


    if ( _committedBlockID <= getLastCommittedBlockID() )
        return;

    CHECK_STATE(
        _committedBlockID == ( getLastCommittedBlockID() + 1 ) || getLastCommittedBlockID() == 0 );

    try {
        ptr< BlockProposal > committedProposal = nullptr;

        lastCommittedBlockTimeStamp = _committedTimeStamp;
        lastCommittedBlockTimeStampMs = _committedTimeStampMs;

        if (_proposerIndex > 0) {
            committedProposal = getNode()->getBlockProposalDB()->getBlockProposal(
                _committedBlockID, _proposerIndex );
        } else {
            committedProposal = createEmptyBlockProposal(_committedBlockID);
        }
        CHECK_STATE( committedProposal );

        auto newCommittedBlock = CommittedBlock::makeObject( committedProposal, _thresholdSig );

        processCommittedBlock( newCommittedBlock );

        proposeNextBlock( _committedTimeStamp, _committedTimeStampMs );

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

void Schain::proposeNextBlock(
    uint64_t _previousBlockTimeStamp, uint32_t _previousBlockTimeStampMs ) {


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
                _proposedBlockID, _previousBlockTimeStamp, _previousBlockTimeStampMs );
        }


        CHECK_STATE( myProposal );

        CHECK_STATE( myProposal->getProposerIndex() == getSchainIndex() );
        CHECK_STATE( myProposal->getSignature() != "");




        proposedBlockArrived( myProposal );

        LOG( debug, "PROPOSING BLOCK NUMBER:" + to_string( _proposedBlockID ) );

        auto db = getNode()->getProposalHashDB();

        db->checkAndSaveHash( _proposedBlockID, getSchainIndex(), myProposal->getHash()->toHex() );

        blockProposalClient->enqueueItem( myProposal );

        auto [mySig, ecdsaSig, pubKey, pubKeySig] = getSchain()->getCryptoManager()->signDAProof( myProposal );

        CHECK_STATE(mySig);

        //make compiler happy
        ecdsaSig = ""; pubKey = ""; pubKeySig = "";

        getSchain()->daProofSigShareArrived( mySig, myProposal );

    } catch ( ExitRequestedException& e ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}

void Schain::processCommittedBlock(const ptr< CommittedBlock >& _block ) {
    CHECK_ARGUMENT( _block );
    MONITOR2( __CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime() )

    checkForExit();

    LOCK( m )

    try {
        CHECK_STATE( getLastCommittedBlockID() + 1 == _block->getBlockID() );

        totalTransactions += _block->getTransactionList()->size();

        auto h = _block->getHash()->toHex().substr( 0, 8 );
        LOG( info,
            "BLOCK_COMMIT: PRPSR:" + to_string( _block->getProposerIndex() ) +
                ":BID: " + to_string( _block->getBlockID() ) +
                ":ROOT:" + _block->getStateRoot().convert_to<string>() + ":HASH:" + h +
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
                to_string( getSchain()->getNode()->getNetwork()->computeTotalDelayedSends() ) );


        saveBlock( _block );

        pushBlockToExtFace( _block );

        lastCommittedBlockID++;
        lastCommitTimeMs = Time::getCurrentTimeMs();

    } catch ( ExitRequestedException& e ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}

void Schain::saveBlock(const ptr< CommittedBlock >& _block ) {
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


void Schain::pushBlockToExtFace(const ptr< CommittedBlock >& _block ) {
    CHECK_ARGUMENT( _block );

    MONITOR2( __CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime() )

    checkForExit();

    try {
        auto tv = _block->getTransactionList()->createTransactionVector();

        // auto next_price = // VERIFY PRICING

        this->pricingAgent->calculatePrice(
            *tv, _block->getTimeStamp(), _block->getTimeStampMs(), _block->getBlockID() );

        auto currentPrice = this->pricingAgent->readPrice( _block->getBlockID() - 1 );


        if ( extFace ) {
            extFace->createBlock( *tv, _block->getTimeStamp(), _block->getTimeStampMs(),
                ( __uint64_t ) _block->getBlockID(), currentPrice, _block->getStateRoot() );
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


    CHECK_STATE( blockConsensusInstance);
    CHECK_STATE(_proposalVector);

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


void Schain::daProofArrived(const ptr< DAProof >& _daProof ) {
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


void Schain::proposedBlockArrived(const ptr< BlockProposal >& _proposal ) {
    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    if ( _proposal->getBlockID() <= getLastCommittedBlockID() )
        return;

    CHECK_STATE( _proposal->getSignature() != "" );

    getNode()->getBlockProposalDB()->addBlockProposal( _proposal );
}


void Schain::bootstrap( block_id _lastCommittedBlockID, uint64_t _lastCommittedBlockTimeStamp ) {
    LOG(info, "Bootstrapping consensus ...");
    auto _lastCommittedBlockIDInConsensus = getNode()->getBlockDB()->readLastCommittedBlockID();
    LOG(info, "Check the consensus database for corruption ...");
    this->startingFromCorruptState = fixCorruptStateIfNeeded(_lastCommittedBlockIDInConsensus);
    if (startingFromCorruptState) {
        LOG(warn, "Corrupt consensus database has been repaired successfully."
            "Starting from repaired consensus database.");
    }

    LOG( info,
        "Last committed block in consensus:" + to_string( _lastCommittedBlockIDInConsensus ) );

    checkForExit();

    // Step 1: solve block id  mismatch problems


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
        CHECK_STATE(!bootStrapped);
        bootStrapped = true;
        bootstrapBlockID.store( ( uint64_t ) _lastCommittedBlockID );
        CHECK_STATE(_lastCommittedBlockTimeStamp < ( uint64_t ) 2 * MODERN_TIME );

        LOCK( m )

        ptr< BlockProposal > committedProposal = nullptr;

        lastCommittedBlockID = ( uint64_t ) _lastCommittedBlockID;
        lastCommitTimeMs = ( uint64_t ) Time::getCurrentTimeMs();
        lastCommittedBlockTimeStamp = _lastCommittedBlockTimeStamp;
        lastCommittedBlockTimeStampMs = 0;


        LOG( info, "Jump starting the system with block:" + to_string( _lastCommittedBlockID ) );
        if ( getLastCommittedBlockID() == 0 )
            this->pricingAgent->calculatePrice( ConsensusExtFace::transactions_vector(), 0, 0, 0 );

        proposeNextBlock( lastCommittedBlockTimeStamp, lastCommittedBlockTimeStampMs );

        rebroadcastAllMessagesForCurrentBlock();

    } catch ( exception& e ) {
        SkaleException::logNested( e );
        return;
    }
}
void Schain::rebroadcastAllMessagesForCurrentBlock() const {
    auto proposalVector = getNode()->getProposalVectorDB()->getVector( lastCommittedBlockID + 1 );
    if ( proposalVector ) {
        LOG(info, "Rebroadcasting messages for the current block");
        auto messages =
            getNode()->getOutgoingMsgDB()->getMessages( lastCommittedBlockID + 1 );
        for ( auto&& m : *messages ) {
            getNode()->getNetwork()->rebroadcastMessage(m);
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


void Schain::constructServers(const ptr< Sockets >& _sockets ) {
    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    blockProposalServerAgent =
        make_shared< BlockProposalServerAgent >( *this, _sockets->blockProposalSocket );
    catchupServerAgent = make_shared< CatchupServerAgent >( *this, _sockets->catchupSocket );
}

ptr< BlockProposal > Schain::createEmptyBlockProposal( block_id _blockId ) {
    uint64_t sec = lastCommittedBlockTimeStamp;
    uint64_t ms = lastCommittedBlockTimeStampMs;

    // Set time for an empty block to be 1 ms more than previous block
    if ( ms == 999 ) {
        sec++;
        ms = 0;
    } else {
        ms++;
    }

    auto myProposal = getNode()->getBlockProposalDB()->getBlockProposal(_blockId,
        getSchainIndex());

    CHECK_STATE(myProposal);

    return make_shared< ReceivedBlockProposal >( *this, _blockId, sec, ms, myProposal->getStateRoot() );
}


void Schain::finalizeDecidedAndSignedBlock(
    block_id _blockId, schain_index _proposerIndex, const ptr< ThresholdSignature >& _thresholdSig ) {
    CHECK_ARGUMENT( _thresholdSig != nullptr );


    MONITOR2( __CLASS_NAME__, __FUNCTION__, getMaxExternalBlockProcessingTime() )


    if ( _blockId <= getLastCommittedBlockID() ) {
        LOG( info, "Ignoring old block decide, already got this through catchup: BID:" +
                       to_string( _blockId ) + ":PRP:" + to_string( _proposerIndex ) );
        return;
    }


    LOG( info, "BLOCK_SIGNED: Now finalizing block ... BID:" + to_string( _blockId ) );

    ptr< BlockProposal > proposal = nullptr;

    bool haveProof = false;

    try {
        if ( _proposerIndex == 0 ) {
            proposal = createEmptyBlockProposal( _blockId );
            haveProof = true;  // empty proposals donot need DAP proofs
        } else {
            proposal =
                getNode()->getBlockProposalDB()->getBlockProposal( _blockId, _proposerIndex );
            if ( proposal != nullptr ) {
                haveProof = getNode()->getDaProofDB()->haveDAProof( proposal );
            }
        }


        if ( !haveProof ||  // a proposal without a  DA proof is not trusted and has to be
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

            if ( proposal != nullptr )  // Nullptr means catchup happened first
                getNode()->getBlockProposalDB()->addBlockProposal( proposal );
        }

        if ( proposal != nullptr )
            blockCommitArrived( _blockId, _proposerIndex, proposal->getTimeStamp(),
                proposal->getTimeStampMs(), _thresholdSig );

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
    if ( getNode()->getBlockDB()->unfinishedBlockExists( nextBlock )) {
        return true;
    } else {
        return false;
    }
}


bool Schain::isStartingFromCorruptState() const {
    return startingFromCorruptState;
}
