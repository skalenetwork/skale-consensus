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

    @file BlockProposalPusherAgent.cpp
    @author Stan Kladko
    @date 2018
*/

#include "Agent.h"
#include "Log.h"
#include "SkaleCommon.h"
#include "exceptions/FatalError.h"
#include "thirdparty/json.hpp"

#include "abstracttcpserver/ConnectionStatus.h"
#include "crypto/BLAKE3Hash.h"
#include "datastructures/PartialHashesList.h"
#include "datastructures/Transaction.h"
#include "datastructures/TransactionList.h"


#include "chains/Schain.h"
#include "crypto/CryptoManager.h"
#include "crypto/ThresholdSigShare.h"
#include "datastructures/BlockProposal.h"
#include "datastructures/CommittedBlock.h"
#include "datastructures/DAProof.h"
#include "exceptions/NetworkProtocolException.h"
#include "headers/BlockProposalRequestHeader.h"
#include "headers/FinalProposalResponseHeader.h"
#include "headers/MissingTransactionsRequestHeader.h"
#include "headers/MissingTransactionsResponseHeader.h"
#include "headers/SubmitDAProofRequestHeader.h"
#include "network/ClientSocket.h"
#include "network/IO.h"
#include "network/Network.h"
#include "network/ServerConnection.h"
#include "node/Node.h"
#include "node/NodeInfo.h"
#include "pendingqueue/PendingTransactionsAgent.h"

#include "BlockProposalClientAgent.h"
#include "BlockProposalPusherThreadPool.h"
#include "abstracttcpclient/AbstractClientAgent.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/PingException.h"

BlockProposalClientAgent::BlockProposalClientAgent( Schain& _sChain )
    : AbstractClientAgent( _sChain, PROPOSAL ) {
    try {
        LOG( debug, "Constructing blockProposalPushAgent" );

        this->blockProposalThreadPool = make_shared< BlockProposalPusherThreadPool >(
            num_threads( ( uint64_t ) _sChain.getNodeCount() ), this );
        blockProposalThreadPool->startService();
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( FatalError( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


ptr< MissingTransactionsRequestHeader >
BlockProposalClientAgent::readMissingTransactionsRequestHeader(
    const ptr< ClientSocket >& _socket ) {
    auto js =
        sChain->getIo()->readJsonHeader( _socket->getDescriptor(), "Read missing trans request",
            10, _socket->getIP());
    auto mtrh = make_shared< MissingTransactionsRequestHeader >();

    auto status = ( ConnectionStatus ) Header::getUint64( js, "status" );
    auto substatus = ( ConnectionSubStatus ) Header::getUint64( js, "substatus" );
    mtrh->setStatusSubStatus( status, substatus );

    auto count = ( uint64_t ) Header::getUint64( js, "count" );
    mtrh->setMissingTransactionsCount( count );

    mtrh->setComplete();
    LOG( trace, "Push agent processed missing transactions header" );
    return mtrh;
}

ptr< FinalProposalResponseHeader >
BlockProposalClientAgent::readAndProcessFinalProposalResponseHeader(
    const ptr< ClientSocket >& _socket ) {
    auto js =
        sChain->getIo()->readJsonHeader( _socket->getDescriptor(), "Read final response header",
            10,
            _socket->getIP());

    auto status = ( ConnectionStatus ) Header::getUint64( js, "status" );
    auto subStatus = ( ConnectionSubStatus ) Header::getUint64( js, "substatus" );

    if ( status == CONNECTION_SUCCESS ) {
        return make_shared< FinalProposalResponseHeader >( Header::getString( js, "sss" ),
            Header::getString( js, "sig" ), Header::getString( js, "pk" ),
            Header::getString( js, "pks" ) );
    } else {
        LOG( err, "Proposal push failed:" + to_string( status ) + ":" + to_string( subStatus ) );
        return make_shared< FinalProposalResponseHeader >( status, subStatus );
    }
}

pair< ConnectionStatus, ConnectionSubStatus > BlockProposalClientAgent::sendItemImpl(
    const ptr< SendableItem >& _item, const ptr< ClientSocket >& _socket, schain_index _index ) {
    CHECK_ARGUMENT( _item );
    CHECK_ARGUMENT( _socket );

    auto _proposal = dynamic_pointer_cast< BlockProposal >( _item );
    if ( _proposal != nullptr ) {
        return sendBlockProposal( _proposal, _socket, _index );
    } else {
        auto _daProof = dynamic_pointer_cast< DAProof >( _item );
        CHECK_STATE( _daProof );  // a sendable item is either DAProof or Proposal
        return sendDAProof( _daProof, _socket );
    }
}

ptr< BlockProposal > BlockProposalClientAgent::corruptProposal(
    const ptr< BlockProposal >& _proposal, schain_index _index ) {
    if ( ( uint64_t ) _index % 2 == 0 ) {
        auto proposal2 = make_shared< BlockProposal >( _proposal->getSchainID(),
            _proposal->getProposerNodeID(), _proposal->getBlockID(), _proposal->getProposerIndex(),
            make_shared< TransactionList >( make_shared< vector< ptr< Transaction > > >() ),
            _proposal->getStateRoot(), MODERN_TIME + 1, 1, nullptr,
            getSchain()->getCryptoManager() );
        return proposal2;
    } else {
        return _proposal;
    }
}


pair< ConnectionStatus, ConnectionSubStatus > BlockProposalClientAgent::sendBlockProposal(
    const ptr< BlockProposal >& _proposal, const ptr< ClientSocket >& _socket,
    schain_index _index ) {
    CHECK_ARGUMENT( _proposal );
    CHECK_ARGUMENT( _socket );

    auto proposalCopy = _proposal;


    INJECT_TEST( CORRUPT_PROPOSAL_TEST, proposalCopy = corruptProposal( _proposal, _index ) )

    LOG( trace, "Proposal step 0: Starting block proposal" );


    ptr< Header > header = BlockProposal::createBlockProposalHeader( sChain, proposalCopy );

    CHECK_STATE( header );

    try {
        getSchain()->getIo()->writeHeader( _socket, header );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( NetworkProtocolException( "Could not write header", __CLASS_NAME__ ) );
    }


    LOG( trace, "Proposal step 1: wrote proposal header" );

    auto response =
        sChain->getIo()->readJsonHeader( _socket->getDescriptor(), "Read proposal resp" ,
            10,
            _socket->getIP());


    LOG( trace, "Proposal step 2: read proposal response" );

    pair< ConnectionStatus, ConnectionSubStatus > result = {
        ConnectionStatus::CONNECTION_STATUS_UNKNOWN,
        ConnectionSubStatus::CONNECTION_SUBSTATUS_UNKNOWN };


    try {
        result.first = ( ConnectionStatus ) Header::getUint64( response, "status" );
        result.second = ( ConnectionSubStatus ) Header::getUint64( response, "substatus" );
    } catch ( ... ) {
    }


    if ( result.first != CONNECTION_PROCEED ) {
        LOG( trace, "Proposal Server terminated proposal push:" + to_string( result.first ) + ":" +
                        to_string( result.second ) );
        return result;
    }

    auto partialHashesList = _proposal->createPartialHashesList();

    CHECK_STATE( partialHashesList );

    if ( partialHashesList->getTransactionCount() > 0 ) {
        try {
            getSchain()->getIo()->writeBytesVector(
                _socket->getDescriptor(), partialHashesList->getPartialHashes() );
        } catch ( ExitRequestedException& ) {
            throw;
        } catch ( ... ) {
            auto errStr = "Unexpected disconnect writing block data";
            throw_with_nested( NetworkProtocolException( errStr, __CLASS_NAME__ ) );
        }
    }


    LOG( trace, "Proposal step 3: sent partial hashes" );

    ptr< MissingTransactionsRequestHeader > missingTransactionHeader;

    try {
        missingTransactionHeader = readMissingTransactionsRequestHeader( _socket );
        CHECK_STATE( missingTransactionHeader );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        auto errStr = "Could not read missing transactions request header";
        throw_with_nested( NetworkProtocolException( errStr, __CLASS_NAME__ ) );
    }

    auto count = missingTransactionHeader->getMissingTransactionsCount();

    if ( count == 0 ) {
        LOG( trace, "Proposal complete::no missing transactions" );

    } else {
        ptr< unordered_set< ptr< partial_sha_hash >, PendingTransactionsAgent::Hasher,
            PendingTransactionsAgent::Equal > >
            missingHashes;

        try {
            missingHashes = readMissingHashes( _socket, count );
            CHECK_STATE( missingHashes );
        } catch ( ExitRequestedException& ) {
            throw;
        } catch ( ... ) {
            auto errStr = "Could not read missing hashes";
            throw_with_nested( NetworkProtocolException( errStr, __CLASS_NAME__ ) );
        }


        LOG( trace, "Proposal step 4: read missing transaction hashes" );


        auto missingTransactions = make_shared< vector< ptr< Transaction > > >();
        auto missingTransactionsSizes = make_shared< vector< uint64_t > >();

        for ( auto&& transaction : *_proposal->getTransactionList()->getItems() ) {
            if ( missingHashes->count( transaction->getPartialHash() ) ) {
                missingTransactions->push_back( transaction );
                missingTransactionsSizes->push_back( transaction->getSerializedSize( false ) );
            }
        }

        CHECK_STATE2( missingTransactions->size() == count,
            "Transactions:" + to_string( missingTransactions->size() ) + ":" + to_string( count ) );


        auto mtrh = make_shared< MissingTransactionsResponseHeader >( missingTransactionsSizes );

        try {
            getSchain()->getIo()->writeHeader( _socket, mtrh );
        } catch ( ExitRequestedException& ) {
            throw;
        } catch ( ... ) {
            auto errString =
                "Proposal: unexpected server disconnect writing missing txs response header";
            throw_with_nested( NetworkProtocolException( errString, __CLASS_NAME__ ) );
        }


        LOG( trace, "Proposal step 5: sent missing transactions header" );


        auto missingTransactionsList = make_shared< TransactionList >( missingTransactions );

        try {
            getSchain()->getIo()->writeBytesVector(
                _socket->getDescriptor(), missingTransactionsList->serialize( false ) );
        } catch ( ExitRequestedException& ) {
            throw;
        } catch ( ... ) {
            auto errString = "Proposal: unexpected server disconnect  writing missing hashes";
            throw_with_nested( NetworkProtocolException( errString, __CLASS_NAME__ ) );
        }

        LOG( trace, "Proposal step 6: sent missing transactions" );
    }

    auto finalHeader = readAndProcessFinalProposalResponseHeader( _socket );

    CHECK_STATE( finalHeader );


    pair< ConnectionStatus, ConnectionSubStatus > finalResult = {
        ConnectionStatus::CONNECTION_STATUS_UNKNOWN,
        ConnectionSubStatus::CONNECTION_SUBSTATUS_UNKNOWN };

    try {
        finalResult = finalHeader->getStatusSubStatus();
    } catch ( ... ) {
    }

    if ( finalResult.first != ConnectionStatus::CONNECTION_SUCCESS )
        return finalResult;


    auto sigShare =
        getSchain()->getCryptoManager()->createDAProofSigShare( finalHeader->getSigShare(),
            _proposal->getSchainID(), _proposal->getBlockID(), _index, false );

    auto h = _proposal->getHash();

    getSchain()->getCryptoManager()->verifyDAProofSigShare(
        sigShare, _index, h, getSchain()->getNodeIDByIndex( _index ), false );

    CHECK_STATE( sigShare );

    auto hash = BLAKE3Hash::merkleTreeMerge( _proposal->getHash(), sigShare->computeHash() );

    auto nodeInfo = getSchain()->getNode()->getNodeInfoByIndex( _index );

    CHECK_STATE( nodeInfo );

    CHECK_STATE( getSchain()->getCryptoManager()->sessionVerifySigAndKey( hash,
        finalHeader->getSignature(), finalHeader->getPublicKey(), finalHeader->getPublicKeySig(),
        _proposal->getBlockID(), nodeInfo->getNodeID() ) );

    getSchain()->daProofSigShareArrived( sigShare, _proposal );

    return finalResult;
}


pair< ConnectionStatus, ConnectionSubStatus > BlockProposalClientAgent::sendDAProof(
    const ptr< DAProof >& _daProof, const ptr< ClientSocket >& _socket ) {
    CHECK_ARGUMENT( _daProof );
    CHECK_ARGUMENT( _socket );


    LOG( trace, "Proposal step 0: Starting block proposal" );

    CHECK_STATE( _daProof );

    auto header =
        make_shared< SubmitDAProofRequestHeader >( *getSchain(), _daProof, _daProof->getBlockId() );

    try {
        getSchain()->getIo()->writeHeader( _socket, header );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( NetworkProtocolException( "Could not write header", __CLASS_NAME__ ) );
    }


    LOG( trace, "DA proof step 1: wrote request header" );

    auto response =
        sChain->getIo()->readJsonHeader( _socket->getDescriptor(), "Read dap proof resp",
            10,
            _socket->getIP());


    LOG( trace, "DAProof step 2: read response" );

    auto status = ConnectionStatus::CONNECTION_STATUS_UNKNOWN;
    auto substatus = ConnectionSubStatus::CONNECTION_SUBSTATUS_UNKNOWN;


    try {
        status = ( ConnectionStatus ) Header::getUint64( response, "status" );
        substatus = ( ConnectionSubStatus ) Header::getUint64( response, "substatus" );
    } catch (...) {
        LOG( err, "Unknown failure submitting DA proof");
        return {status, substatus};
    }

    if ( status == CONNECTION_ERROR ) {
        LOG( err, "Failure submitting DA proof:" + to_string( status ) + ":" + to_string( substatus ) );
    }

    return { status, substatus };
}


ptr< unordered_set< ptr< partial_sha_hash >, PendingTransactionsAgent::Hasher,
    PendingTransactionsAgent::Equal > >

BlockProposalClientAgent::readMissingHashes( const ptr< ClientSocket >& _socket, uint64_t _count ) {
    CHECK_ARGUMENT( _socket );
    CHECK_ARGUMENT( _count > 0 );

    auto bytesToRead = _count * PARTIAL_HASH_LEN;
    auto buffer = make_shared< vector< uint8_t > >( bytesToRead );

    CHECK_STATE( bytesToRead > 0 );


    try {
        getSchain()->getIo()->readBytes( _socket->getDescriptor(), buffer, msg_len( bytesToRead ),
            30);
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        LOG( info, "Could not read partial hashes" );
        throw_with_nested(
            NetworkProtocolException( "Could not read partial data hashes", __CLASS_NAME__ ) );
    }


    auto result = make_shared< unordered_set< ptr< partial_sha_hash >,
        PendingTransactionsAgent::Hasher, PendingTransactionsAgent::Equal > >();


    try {
        for ( uint64_t i = 0; i < _count; i++ ) {
            auto hash = make_shared< partial_sha_hash >();
            for ( size_t j = 0; j < PARTIAL_HASH_LEN; j++ ) {
                hash->at( j ) = buffer->at( PARTIAL_HASH_LEN * i + j );
            }

            result->insert( hash );
        }
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( NetworkProtocolException(
            "Could not read missing transaction hashes:" + to_string( _count ), __CLASS_NAME__ ) );
    }

    return result;
}
