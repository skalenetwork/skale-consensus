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

    @file BlockProposalServerAgent.h
    @author Stan Kladko
    @date 2018
*/

#pragma once

#include "abstracttcpserver/AbstractServerAgent.h"
#include "abstracttcpserver/ConnectionStatus.h"
#include "pendingqueue/PendingTransactionsAgent.h"

class BlockProposalWorkerThreadPool;
class BlockFinalizeResponseHeader;
class BlockProposalRequestHeader;
class SubmitDAProofRequestHeader;
class ReceivedBlockProposal;


class Transaction;

class TransactionList;


class Comparator {
public:
    bool operator()( const ptr< partial_sha_hash >& a, const ptr< partial_sha_hash >& b ) const {
        for ( size_t i = 0; i < PARTIAL_HASH_LEN; i++ ) {
            if ( ( *a )[i] < ( *b )[i] )
                return false;
            if ( ( *b )[i] < ( *a )[i] )
                return true;
        }
        return false;
    }
};


class BlockProposalServerAgent : public AbstractServerAgent {
    ptr< BlockProposalWorkerThreadPool > blockProposalWorkerThreadPool;


    pair< ConnectionStatus, ConnectionSubStatus > processProposalRequest(
        const ptr< ServerConnection >& _connection, nlohmann::json _proposalRequest );

    void processDAProofRequest(
        const ptr< ServerConnection >& _connection, nlohmann::json _daProofRequest );


public:
    BlockProposalServerAgent( Schain& _schain, const ptr< TCPServerSocket >& _s );

    ~BlockProposalServerAgent() override;

    ptr< unordered_map< ptr< partial_sha_hash >, ptr< Transaction >,
        PendingTransactionsAgent::Hasher, PendingTransactionsAgent::Equal > >
    readMissingTransactions( const ptr< ServerConnection >& _connectionEnvelope,
        nlohmann::json missingTransactionsResponseHeader );


    pair< ptr< map< uint64_t, ptr< Transaction > > >,
        ptr< map< uint64_t, ptr< partial_sha_hash > > > >
    getPresentAndMissingTransactions(
        Schain& _sChain, const ptr< Header >, const ptr< PartialHashesList >& _phList );


    BlockProposalWorkerThreadPool* getBlockProposalWorkerThreadPool() const;

    void checkForOldBlock( const block_id& _blockID );

    ptr< Header > createProposalResponseHeader(
        const ptr< ServerConnection >& _connectionEnvelope, BlockProposalRequestHeader& _header );

    ptr< Header > createFinalResponseHeader( const ptr< ReceivedBlockProposal >& _proposal );

    ptr< Header > createDAProofResponseHeader( const ptr< ServerConnection >& _connectionEnvelope,
        const ptr< SubmitDAProofRequestHeader >& _header );


    nlohmann::json readMissingTransactionsResponseHeader(
        const ptr< ServerConnection >& _connectionEnvelope );


    void processNextAvailableConnection( const ptr< ServerConnection >& _connection ) override;

    void signBlock( const ptr< BlockFinalizeResponseHeader >& _responseHeader,
        const ptr< CommittedBlock >& _block ) const;
    void logStateRootMismatchError( BlockProposalRequestHeader& _header, block_id& blockIDInHeader,
        const ptr< BlockProposal >& myBlockProposalForTheSameBlockID );
};
