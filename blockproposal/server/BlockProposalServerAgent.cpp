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

    @file BlockProposalServerAgent.cpp
    @author Stan Kladko
    @date 2018
*/

#include "Agent.h"
#include "Log.h"
#include "SkaleCommon.h"
#include "exceptions/FatalError.h"
#include "thirdparty/json.hpp"

#include "abstracttcpserver/ConnectionStatus.h"
#include "exceptions/CouldNotReadPartialDataHashesException.h"
#include "exceptions/CouldNotSendMessageException.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/InvalidHashException.h"
#include "exceptions/InvalidNodeIDException.h"
#include "exceptions/InvalidSchainException.h"
#include "exceptions/InvalidSchainIndexException.h"
#include "exceptions/InvalidSourceIPException.h"
#include "exceptions/OldBlockIDException.h"
#include "exceptions/PingException.h"
#include "node/NodeInfo.h"
#include "utils/Time.h"

#include "crypto/ConsensusBLSSigShare.h"
#include "crypto/CryptoManager.h"
#include "datastructures/DAProof.h"

#include "blockproposal/pusher/BlockProposalClientAgent.h"
#include "datastructures/TimeStamp.h"
#include "db/BlockProposalDB.h"
#include "db/DAProofDB.h"
#include "pendingqueue/PendingTransactionsAgent.h"

#include "abstracttcpserver/AbstractServerAgent.h"
#include "chains/Schain.h"
#include "crypto/BLAKE3Hash.h"
#include "headers/BlockProposalResponseHeader.h"
#include "headers/FinalProposalResponseHeader.h"
#include "headers/Header.h"
#include "headers/MissingTransactionsRequestHeader.h"
#include "network/IO.h"
#include "network/Network.h"
#include "network/ServerConnection.h"
#include "network/Sockets.h"

#include "node/Node.h"

#include "datastructures/BlockProposal.h"
#include "datastructures/CommittedBlock.h"
#include "datastructures/PartialHashesList.h"
#include "datastructures/ReceivedBlockProposal.h"
#include "datastructures/Transaction.h"
#include "datastructures/TransactionList.h"
#include "db/ProposalHashDB.h"
#include "headers/AbstractBlockRequestHeader.h"
#include "headers/BlockProposalRequestHeader.h"
#include "headers/SubmitDAProofRequestHeader.h"
#include "headers/SubmitDAProofResponseHeader.h"


#include "BlockProposalServerAgent.h"
#include "BlockProposalWorkerThreadPool.h"
#include "crypto/ConsensusBLSSigShare.h"
#include "headers/BlockFinalizeResponseHeader.h"
#include "monitoring/LivelinessMonitor.h"


ptr< unordered_map< ptr< partial_sha_hash >, ptr< Transaction >, PendingTransactionsAgent::Hasher,
    PendingTransactionsAgent::Equal > >
BlockProposalServerAgent::readMissingTransactions(
    const ptr< ServerConnection >& _connectionEnvelope,
    nlohmann::json missingTransactionsResponseHeader ) {
    CHECK_ARGUMENT( _connectionEnvelope );
    CHECK_STATE( missingTransactionsResponseHeader > 0 );

    auto transactionSizes = make_shared< vector< uint64_t > >();

    nlohmann::json jsonSizes = missingTransactionsResponseHeader["sizes"];

    if ( !jsonSizes.is_array() ) {
        BOOST_THROW_EXCEPTION(
            NetworkProtocolException( "jsonSizes is not an array", __CLASS_NAME__ ) );
    };


    size_t totalSize = 2;  // account for starting and ending < >

    for ( auto&& size : jsonSizes ) {
        transactionSizes->push_back( size );
        totalSize += ( size_t ) size;
    }

    auto serializedTransactions = make_shared< vector< uint8_t > >( totalSize );


    try {
        getSchain()->getIo()->readBytes(
            _connectionEnvelope, serializedTransactions, msg_len( totalSize ) );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION(
            NetworkProtocolException( "Could not read serialized exceptions", __CLASS_NAME__ ) );
    }

    auto list = TransactionList::deserialize( transactionSizes, serializedTransactions, 0, false );

    CHECK_STATE( list );

    auto trs = list->getItems();

    CHECK_STATE( trs );

    auto missed = make_shared< unordered_map< ptr< partial_sha_hash >, ptr< Transaction >,
        PendingTransactionsAgent::Hasher, PendingTransactionsAgent::Equal > >();

    for ( auto&& t : *trs ) {
        ( *missed )[t->getPartialHash()] = t;
    }

    return missed;
}

pair< ptr< map< uint64_t, ptr< Transaction > > >, ptr< map< uint64_t, ptr< partial_sha_hash > > > >
BlockProposalServerAgent::getPresentAndMissingTransactions(
    Schain& _sChain, const ptr< Header > /*tcpHeader*/, const ptr< PartialHashesList >& _phList ) {
    CHECK_ARGUMENT( _phList );

    LOG( debug, "Calculating missing hashes" );

    auto transactionsCount = _phList->getTransactionCount();

    auto presentTransactions = make_shared< map< uint64_t, ptr< Transaction > > >();
    auto missingHashes = make_shared< map< uint64_t, ptr< partial_sha_hash > > >();

    for ( uint64_t i = 0; i < transactionsCount; i++ ) {
        auto hash = _phList->getPartialHash( i );
        CHECK_STATE( hash );
        auto transaction =
            _sChain.getPendingTransactionsAgent()->getKnownTransactionByPartialHash( hash );
        if ( transaction == nullptr ) {
            ( *missingHashes )[i] = hash;
        } else {
            ( *presentTransactions )[i] = transaction;
        }
    }

    return { presentTransactions, missingHashes };
}


BlockProposalServerAgent::BlockProposalServerAgent(
    Schain& _schain, const ptr< TCPServerSocket >& _s )
    : AbstractServerAgent( "BlockPropSrv", _schain, _s ) {
    blockProposalWorkerThreadPool =
        make_shared< BlockProposalWorkerThreadPool >( num_threads( 1 ), this );
    blockProposalWorkerThreadPool->startService();
    createNetworkReadThread();
}

BlockProposalServerAgent::~BlockProposalServerAgent() {}


void BlockProposalServerAgent::processNextAvailableConnection(
    const ptr< ServerConnection >& _connection ) {
    MONITOR( __CLASS_NAME__, __FUNCTION__ );

    CHECK_ARGUMENT( _connection );

    try {
        sChain->getIo()->readMagic( _connection->getDescriptor() );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( PingException& ) {
        return;
    } catch ( ... ) {
        throw_with_nested(
            NetworkProtocolException( "Could not read magic number", __CLASS_NAME__ ) );
    }


    nlohmann::json clientRequest = nullptr;

    try {
        clientRequest = getSchain()->getIo()->readJsonHeader(
            _connection->getDescriptor(), "Read proposal req" );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested(
            CouldNotSendMessageException( "Could not read proposal request", __CLASS_NAME__ ) );
    }


    auto type = Header::getString( clientRequest, "type" );

    CHECK_STATE( !type.empty() );

    if ( strcmp( type.data(), Header::BLOCK_PROPOSAL_REQ ) == 0 ) {
        processProposalRequest( _connection, clientRequest );
    } else if ( strcmp( type.data(), Header::DA_PROOF_REQ ) == 0 ) {
        processDAProofRequest( _connection, clientRequest );
    } else {
        BOOST_THROW_EXCEPTION(
            NetworkProtocolException( "Uknown request type:" + type, __CLASS_NAME__ ) );
    }
}


void BlockProposalServerAgent::processDAProofRequest(
    const ptr< ServerConnection >& _connection, nlohmann::json _daProofRequest ) {
    CHECK_ARGUMENT( _connection );

    ptr< SubmitDAProofRequestHeader > requestHeader = nullptr;
    ptr< Header > responseHeader = nullptr;

    try {
        requestHeader = make_shared< SubmitDAProofRequestHeader >(
            _daProofRequest, getSchain()->getNodeCount() );
        responseHeader = this->createDAProofResponseHeader( _connection, requestHeader );

        CHECK_STATE( responseHeader );

    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested(
            NetworkProtocolException( "Couldnt create DAProof response header", __CLASS_NAME__ ) );
    }

    try {
        send( _connection, responseHeader );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested(
            NetworkProtocolException( "Couldnt send daProof response header", __CLASS_NAME__ ) );
    }

    LOG( trace, "Got DA proof" );
}

pair< ConnectionStatus, ConnectionSubStatus > BlockProposalServerAgent::processProposalRequest(
    const ptr< ServerConnection >& _connection, nlohmann::json _proposalRequest ) {
    CHECK_ARGUMENT( _connection );

    ptr< BlockProposalRequestHeader > requestHeader = nullptr;
    ptr< Header > responseHeader = nullptr;

    try {
        requestHeader = make_shared< BlockProposalRequestHeader >(
            _proposalRequest, getSchain()->getNodeCount() );
        responseHeader = createProposalResponseHeader( _connection, *requestHeader );
        CHECK_STATE( responseHeader );

    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested(
            NetworkProtocolException( "Couldnt create proposal response header", __CLASS_NAME__ ) );
    }

    try {
        send( _connection, responseHeader );
        if ( responseHeader->getStatusSubStatus().first != CONNECTION_PROCEED ) {
            return responseHeader->getStatusSubStatus();
        }
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested(
            NetworkProtocolException( "Couldnt send proposal response header", __CLASS_NAME__ ) );
    }

    ptr< PartialHashesList > partialHashesList = nullptr;

    try {
        partialHashesList = readPartialHashes( _connection, requestHeader->getTxCount() );
        CHECK_STATE( partialHashesList );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested(
            NetworkProtocolException( "Could not read partial hashes", __CLASS_NAME__ ) );
    }

    auto result = getPresentAndMissingTransactions( *sChain, responseHeader, partialHashesList );

    auto presentTransactions = result.first;
    auto missingTransactionHashes = result.second;

    CHECK_STATE( presentTransactions );
    CHECK_STATE( missingTransactionHashes );

    auto missingHashesRequestHeader =
        make_shared< MissingTransactionsRequestHeader >( missingTransactionHashes );

    try {
        send( _connection, missingHashesRequestHeader );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( CouldNotSendMessageException(
            "Could not send missing hashes request requestHeader", __CLASS_NAME__ ) );
    }


    ptr< unordered_map< ptr< partial_sha_hash >, ptr< Transaction >,
        PendingTransactionsAgent::Hasher, PendingTransactionsAgent::Equal > >
        missingTransactions = nullptr;

    if ( missingTransactionHashes->size() == 0 ) {
        LOG( debug, "Server: No missing partial hashes" );
    } else {
        LOG( debug, "Server: missing partial hashes" );
        try {
            getSchain()->getIo()->writePartialHashes(
                _connection->getDescriptor(), missingTransactionHashes );
        } catch ( ExitRequestedException& ) {
            throw;
        } catch ( ... ) {
            BOOST_THROW_EXCEPTION( CouldNotSendMessageException(
                "Could not send missing hashes  requestHeader", __CLASS_NAME__ ) );
        }


        auto missingMessagesResponseHeader =
            this->readMissingTransactionsResponseHeader( _connection );

        missingTransactions = readMissingTransactions( _connection, missingMessagesResponseHeader );

        if ( missingTransactions == nullptr ) {
            BOOST_THROW_EXCEPTION( CouldNotReadPartialDataHashesException(
                "Null missing transactions", __CLASS_NAME__ ) );
        }


        for ( auto&& item : *missingTransactions ) {
            CHECK_STATE( item.second );
            sChain->getPendingTransactionsAgent()->pushKnownTransaction( item.second );
        }
    }

    LOG( debug, "Storing block proposal" );

    auto transactions = make_shared< vector< ptr< Transaction > > >();

    auto transactionCount = partialHashesList->getTransactionCount();

    for ( uint64_t i = 0; i < transactionCount; i++ ) {
        auto partialHash = partialHashesList->getPartialHash( i );
        CHECK_STATE( partialHash );

        ptr< Transaction > transaction;

        if ( presentTransactions->count( i ) > 0 ) {
            transaction = presentTransactions->at( i );
        } else {
            transaction = ( *missingTransactions )[partialHash];
        };

        if ( transaction == nullptr ) {
            checkForOldBlock( requestHeader->getBlockId() );
            CHECK_STATE( missingTransactions );

            if ( missingTransactions->count( partialHash ) > 0 ) {
                LOG( err, "Found in missing" );
            }

            CHECK_STATE( false );
        }

        CHECK_STATE( transactions != nullptr );

        transactions->push_back( transaction );
    }

    CHECK_STATE( transactionCount == 0 || transactions->at( ( uint64_t ) transactionCount - 1 ) );
    CHECK_STATE( requestHeader->getTimeStamp() > 0 );

    auto transactionList = make_shared< TransactionList >( transactions );

    auto proposal = make_shared< ReceivedBlockProposal >( *sChain, requestHeader->getBlockId(),
        requestHeader->getProposerIndex(), transactionList, requestHeader->getStateRoot(),
        requestHeader->getTimeStamp(), requestHeader->getTimeStampMs(), requestHeader->getHash(),
        requestHeader->getSignature() );

    ptr< Header > finalResponseHeader = nullptr;

    try {
        if ( !getSchain()->getCryptoManager()->verifyProposalECDSA(
                 proposal, requestHeader->getHash(), requestHeader->getSignature() ) ) {
            finalResponseHeader = make_shared< FinalProposalResponseHeader >(
                CONNECTION_ERROR, CONNECTION_SIGNATURE_DID_NOT_VERIFY );
            goto err;
        }

        finalResponseHeader = createFinalResponseHeader( proposal );

        CHECK_STATE( finalResponseHeader );
        CHECK_STATE( proposal );

        sChain->proposedBlockArrived( proposal );


    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( NetworkProtocolException(
            "Couldnt create/send final response header", __CLASS_NAME__ ) );
    }

err:

    CHECK_STATE( finalResponseHeader );

    send( _connection, finalResponseHeader );

    return finalResponseHeader->getStatusSubStatus();
}


void BlockProposalServerAgent::checkForOldBlock( const block_id& _blockID ) {
    LOG( debug, "BID:" + to_string( _blockID ) +
                    ":CBID:" + to_string( getSchain()->getLastCommittedBlockID() ) +
                    ":MQ:" + to_string( getSchain()->getMessagesCount() ) );
    if ( _blockID <= getSchain()->getLastCommittedBlockID() )
        BOOST_THROW_EXCEPTION(
            OldBlockIDException( "Old block ID", nullptr, nullptr, __CLASS_NAME__ ) );
}


ptr< Header > BlockProposalServerAgent::createProposalResponseHeader(
    const ptr< ServerConnection >&, BlockProposalRequestHeader& _header ) {
    auto responseHeader = make_shared< BlockProposalResponseHeader >();

    if ( ( uint64_t ) sChain->getSchainID() != ( uint64_t ) _header.getSchainId() ) {
        responseHeader->setStatusSubStatus( CONNECTION_ERROR, CONNECTION_ERROR_UNKNOWN_SCHAIN_ID );
        responseHeader->setComplete();
        LOG( err, "Incorrect schain " + to_string( _header.getSchainId() ) );
        return responseHeader;
    };


    ptr< NodeInfo > nmi = sChain->getNode()->getNodeInfoById( _header.getProposerNodeId() );

    if ( nmi == nullptr ) {
        responseHeader->setStatusSubStatus(
            CONNECTION_ERROR, CONNECTION_ERROR_DONT_KNOW_THIS_NODE );
        responseHeader->setComplete();
        LOG( err, "Could not find node info for NODE_ID:" +
                      to_string( ( uint64_t ) _header.getProposerNodeId() ) );
        return responseHeader;
    }

    auto blockIDInHeader = _header.getBlockId();

    if ( nmi->getSchainIndex() != ( uint64_t ) _header.getProposerIndex() ) {
        responseHeader->setStatusSubStatus( CONNECTION_ERROR, CONNECTION_ERROR_INVALID_NODE_INDEX );
        responseHeader->setComplete();
        LOG( err, "Node schain index does not match " + _header.getProposerIndex() );
        return responseHeader;
    }

    if ( sChain->getLastCommittedBlockID() >= blockIDInHeader ) {
        responseHeader->setStatusSubStatus(
            CONNECTION_DISCONNECT, CONNECTION_BLOCK_PROPOSAL_TOO_LATE );
        responseHeader->setComplete();
        return responseHeader;
    }

    if ( ( uint64_t ) sChain->getLastCommittedBlockID() + 1 < ( uint64_t ) blockIDInHeader ||
         sChain->getBlockProposal( blockIDInHeader, sChain->getSchainIndex() ) == nullptr ) {
        responseHeader->setStatusSubStatus(
            CONNECTION_RETRY_LATER, CONNECTION_BLOCK_PROPOSAL_IN_THE_FUTURE );
        responseHeader->setComplete();
        return responseHeader;
    }

    if ( sChain->getNode()->getDaProofDB()->isEnoughProofs( blockIDInHeader ) ) {
        responseHeader->setStatusSubStatus(
            CONNECTION_DISCONNECT, CONNECTION_ALREADY_HAVE_ENOUGH_PROPOSALS_FOR_THIS_BLOCK_ID );
        responseHeader->setComplete();;
        return responseHeader;
    }


    auto myBlockProposalForTheSameBlockID =
        sChain->getBlockProposal( blockIDInHeader, sChain->getSchainIndex() );


    if ( myBlockProposalForTheSameBlockID == nullptr ) {
        // did not create proposal yet, ask the client to retry later
        responseHeader->setStatusSubStatus(
            CONNECTION_RETRY_LATER, CONNECTION_BLOCK_PROPOSAL_IN_THE_FUTURE );
        responseHeader->setComplete();
        return responseHeader;
    }


    if ( blockIDInHeader > 1 &&
        _header.getStateRoot() != myBlockProposalForTheSameBlockID->getStateRoot()){
        responseHeader->setStatusSubStatus(
            CONNECTION_ERROR, CONNECTION_PROPOSAL_STATE_ROOT_DOES_NOT_MATCH );
        responseHeader->setComplete();
        LOG( err, "Proposal state root does not match: " );
        LOG( err, " My schain index:"
                      + to_string(getSchain()->getSchainIndex()) + " My root:" +
            myBlockProposalForTheSameBlockID->getStateRoot().str());

        LOG( err, "Sender schain index:"
                  + to_string(_header.getProposerIndex()) +  " Sender root:" +  _header.getStateRoot().str() );


        LOG(err, "State roots of other proposals:");

        auto proposalDB = getNode()->getBlockProposalDB();

        for (uint64_t i = 1; i <= getSchain()->getNodeCount(); i++) {
            auto proposal = proposalDB->getBlockProposal(blockIDInHeader, i);
            if (proposal) {
                LOG( err, "schain_index:"
                          + to_string(proposal->getProposerIndex()) +
                              " root:" +  proposal->getStateRoot().str() );

            }
        }


        return responseHeader;
    }


    if ( _header.getTimeStamp() <= MODERN_TIME ) {
        LOG( info, "Time less than modern time" );
        responseHeader->setStatusSubStatus(
            CONNECTION_ERROR, CONNECTION_ERROR_TIME_LESS_THAN_MODERN_DAY );
        responseHeader->setComplete();
        return responseHeader;
    }

    auto t = Time::getCurrentTimeSec();

    if ( t > ( uint64_t ) MODERN_TIME * 2 ) {
        LOG( info, "Time too far in the future" );
        responseHeader->setStatusSubStatus(
            CONNECTION_ERROR, CONNECTION_ERROR_TIME_TOO_FAR_IN_THE_FUTURE );
        responseHeader->setComplete();
        return responseHeader;
    }

    if ( Time::getCurrentTimeSec() + 1 < _header.getTimeStamp() ) {
        LOG( info, "Incorrect timestamp:" + to_string( _header.getTimeStamp() ) +
                       ":vs:" + to_string( Time::getCurrentTimeSec() ) );
        responseHeader->setStatusSubStatus(
            CONNECTION_ERROR, CONNECTION_ERROR_TIME_STAMP_IN_THE_FUTURE );
        responseHeader->setComplete();
        return responseHeader;
    }

    auto timeStamp = TimeStamp( _header.getTimeStamp(), _header.getTimeStampMs() );

    if ( !( sChain->getLastCommittedBlockTimeStamp() < timeStamp ) ) {
        LOG( info, "Timestamp is less or equal prev block:" + to_string( _header.getTimeStamp() ) +
                       ":vs:" + sChain->getLastCommittedBlockTimeStamp().toString() );

        responseHeader->setStatusSubStatus(
            CONNECTION_DISCONNECT, CONNECTION_ERROR_TIME_STAMP_EARLIER_THAN_COMMITTED );
        responseHeader->setComplete();
        return responseHeader;
    }

    if ( !getSchain()->getNode()->getProposalHashDB()->checkAndSaveHash(
             _header.getBlockId(), _header.getProposerIndex(), _header.getHash() ) ) {
        LOG( info, "Double proposal for block:" + to_string( _header.getBlockId() ) +
                       "  proposer index:" + to_string( _header.getProposerIndex() ) );
        responseHeader->setStatusSubStatus( CONNECTION_DISCONNECT, CONNECTION_DOUBLE_PROPOSAL );
        responseHeader->setComplete();
        return responseHeader;
    }
    responseHeader->setStatusSubStatus( CONNECTION_PROCEED, CONNECTION_OK );
    responseHeader->setComplete();
    return responseHeader;
}

ptr< Header > BlockProposalServerAgent::createDAProofResponseHeader(
    const ptr< ServerConnection >&, const ptr< SubmitDAProofRequestHeader >& _header ) {
    CHECK_ARGUMENT( _header );

    auto responseHeader = make_shared< SubmitDAProofResponseHeader >();

    if ( ( uint64_t ) sChain->getSchainID() != _header->getSchainId() ) {
        responseHeader->setStatusSubStatus( CONNECTION_ERROR, CONNECTION_ERROR_UNKNOWN_SCHAIN_ID );
        BOOST_THROW_EXCEPTION( InvalidSchainException(
            "Incorrect schain " + to_string( _header->getSchainId() ), __CLASS_NAME__ ) );
    };

    auto nodeId = _header->getProposerNodeId();

    ptr< NodeInfo > nmi = sChain->getNode()->getNodeInfoById( nodeId );


    if ( nmi == nullptr ) {
        responseHeader->setStatusSubStatus(
            CONNECTION_ERROR, CONNECTION_ERROR_DONT_KNOW_THIS_NODE );
        BOOST_THROW_EXCEPTION( InvalidNodeIDException(
            "Could not find node info for NODE_ID:" + to_string( ( uint64_t ) nodeId ),
            __CLASS_NAME__ ) );
    }

    if ( ( uint64_t ) nmi->getSchainIndex() != schain_index( _header->getProposerIndex() ) ) {
        responseHeader->setStatusSubStatus( CONNECTION_ERROR, CONNECTION_ERROR_INVALID_NODE_INDEX );
        BOOST_THROW_EXCEPTION( InvalidSchainIndexException(
            "Node schain index does not match " + _header->getProposerIndex(), __CLASS_NAME__ ) );
    }


    if ( sChain->getLastCommittedBlockID() >= _header->getBlockId() ) {
        responseHeader->setStatusSubStatus(
            CONNECTION_DISCONNECT, CONNECTION_BLOCK_PROPOSAL_TOO_LATE );
        responseHeader->setComplete();
        return responseHeader;
    }

    if ( ( uint64_t ) sChain->getLastCommittedBlockID() + 1 < _header->getBlockId() ) {
        responseHeader->setStatusSubStatus(
            CONNECTION_RETRY_LATER, CONNECTION_BLOCK_PROPOSAL_IN_THE_FUTURE );
        responseHeader->setComplete();
        return responseHeader;
    }


    if ( sChain->getNode()->getDaProofDB()->isEnoughProofs( _header->getBlockId() ) ) {
        responseHeader->setStatusSubStatus(
            CONNECTION_DISCONNECT, CONNECTION_ALREADY_HAVE_ENOUGH_PROPOSALS_FOR_THIS_BLOCK_ID );
        responseHeader->setComplete();;
        return responseHeader;
    }



    ptr< BLAKE3Hash > blockHash = nullptr;
    try {
        blockHash = BLAKE3Hash::fromHex( _header->getBlockHash() );
        CHECK_STATE( blockHash );
    } catch ( ... ) {
        responseHeader->setStatusSubStatus( CONNECTION_DISCONNECT, CONNECTION_INVALID_HASH );
        responseHeader->setComplete();
        return responseHeader;
    }

    ptr< ThresholdSignature > sig;

    try {
        sig = getSchain()->getCryptoManager()->verifyDAProofThresholdSig(
            blockHash, _header->getSignature(), _header->getBlockId() );
        CHECK_STATE( sig );

    } catch ( ... ) {
        responseHeader->setStatusSubStatus(
            CONNECTION_DISCONNECT, CONNECTION_SIGNATURE_DID_NOT_VERIFY );
        responseHeader->setComplete();
        return responseHeader;
    }

    auto proposal =
        getSchain()->getBlockProposal( _header->getBlockId(), _header->getProposerIndex() );

    if ( proposal == nullptr ) {
        responseHeader->setStatusSubStatus(
            CONNECTION_DISCONNECT, CONNECTION_DONT_HAVE_PROPOSAL_FOR_THIS_DA_PROOF );
        responseHeader->setComplete();
        return responseHeader;
    }

    if ( proposal->getHash()->toHex() != _header->getBlockHash() ) {
        responseHeader->setStatusSubStatus( CONNECTION_DISCONNECT, CONNECTION_INVALID_HASH );
        responseHeader->setComplete();
        return responseHeader;
    }

    if ( getNode()->getDaProofDB()->haveDAProof( proposal ) ) {
        responseHeader->setStatusSubStatus(
            CONNECTION_DISCONNECT, CONNECTION_ALREADY_HAVE_DA_PROOF );
        responseHeader->setComplete();
        return responseHeader;
    }

    auto proof = make_shared< DAProof >( proposal, sig );

    sChain->daProofArrived( proof );

    responseHeader->setStatusSubStatus( CONNECTION_SUCCESS, CONNECTION_OK );
    responseHeader->setComplete();
    return responseHeader;
}


ptr< Header > BlockProposalServerAgent::createFinalResponseHeader(
    const ptr< ReceivedBlockProposal >& _proposal ) {
    CHECK_ARGUMENT( _proposal );

    auto [sigShare, signature, pubKey, pubKeySig] =
        getSchain()->getCryptoManager()->signDAProof( _proposal );

    auto responseHeader = make_shared< FinalProposalResponseHeader >(
        sigShare->toString(), signature, pubKey, pubKeySig );
    responseHeader->setStatusSubStatus( CONNECTION_SUCCESS, CONNECTION_OK );
    responseHeader->setComplete();
    return responseHeader;
}


nlohmann::json BlockProposalServerAgent::readMissingTransactionsResponseHeader(
    const ptr< ServerConnection >& _connectionEnvelope ) {
    auto js = sChain->getIo()->readJsonHeader(
        _connectionEnvelope->getDescriptor(), "Read missing trans response" );

    return js;
}

ptr< PartialHashesList > AbstractServerAgent::readPartialHashes(
    const ptr< ServerConnection >& _connectionEnvelope, transaction_count _txCount ) {
    CHECK_ARGUMENT( _connectionEnvelope );

    if ( _txCount > ( uint64_t ) getNode()->getMaxTransactionsPerBlock() ) {
        BOOST_THROW_EXCEPTION(
            NetworkProtocolException( "Too many transactions", __CLASS_NAME__ ) );
    }

    auto partialHashesList = make_shared< PartialHashesList >( _txCount );

    if ( ( uint64_t ) _txCount != 0 ) {
        try {
            getSchain()->getIo()->readBytes( _connectionEnvelope,
                partialHashesList->getPartialHashes(),
                msg_len(
                    ( uint64_t ) partialHashesList->getTransactionCount() * PARTIAL_HASH_LEN ) );
        } catch ( ExitRequestedException& ) {
            throw;
        } catch ( ... ) {
            throw_with_nested( CouldNotReadPartialDataHashesException(
                "Could not read partial hashes", __CLASS_NAME__ ) );
        }
    }

    return partialHashesList;
}
