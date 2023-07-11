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

    @file PendingTransactionsAgent.h
    @author Stan Kladko
    @date 2018
*/

#pragma once

#include <boost/functional/hash.hpp>
#include "Agent.h"


class Schain;
class BlockProposal;
class PartialHashesList;
class Transaction;

#include "db/CacheLevelDB.h"

class PendingTransactionsAgent : Agent {
public:
    class Hasher {
    public:
        std::size_t operator()( const ptr< partial_sha_hash >& a ) const {
            size_t hash = 0;
            for ( size_t i = 0; i < PARTIAL_HASH_LEN; i++ ) {
                hash = hash * 31 + ( *a )[i];
            }
            return hash;
        };
    };


    class Equal {
    public:
        std::size_t operator()(
            const ptr< partial_sha_hash >& a, const ptr< partial_sha_hash >& b ) const {
            for ( size_t i = 0; i < PARTIAL_HASH_LEN; i++ ) {
                if ( ( *a )[i] != ( *b )[i] ) {
                    return false;
                }
            }
            return true;
        };
    };


private:
    class Comparator {
    public:
        bool operator()(
            const ptr< partial_sha_hash >& a, const ptr< partial_sha_hash >& b ) const {
            for ( size_t i = 0; i < PARTIAL_HASH_LEN; i++ ) {
                if ( ( *a )[i] < ( *b )[i] )
                    return false;
                if ( ( *b )[i] < ( *a )[i] )
                    return true;
            }
            return false;
        }
    };

    unordered_map< ptr< partial_sha_hash >, ptr< Transaction >, Hasher, Equal > knownTransactions;
    queue< ptr< Transaction > > knownTransactionsQueue;

    uint64_t knownTransactionsTotalSize = 0;

    shared_mutex transactionsMutex;

    transaction_count transactionCounter = 0;

    pair< ptr< vector< ptr< Transaction > > >, u256 > createTransactionsListForProposal(
        uint64_t _maxPendingQueueWaitTimeMs );

    uint64_t transactionListWaitTime = 0;

    uint64_t transactionListReceivedTimeMs = 0;

public:
    explicit PendingTransactionsAgent( Schain& _sChain );

    void pushKnownTransaction( const ptr< Transaction >& _transaction );

    uint64_t getKnownTransactionsSize();

    ptr< Transaction > getKnownTransactionByPartialHash( ptr< partial_sha_hash > hash );

    ptr< BlockProposal > buildBlockProposal(
        block_id _blockID, TimeStamp& _timeStamp, uint64_t _maxPendingQueueWaitTimeMs );

    uint64_t getTransactionListWaitTime() const { return transactionListWaitTime; };

    uint64_t transactionListReceivedTime() const { return transactionListReceivedTimeMs; }

    ~PendingTransactionsAgent() override = default;
};
