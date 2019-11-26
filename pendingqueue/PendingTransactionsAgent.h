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
#include "../Agent.h"


class Schain;
class BlockProposal;
class PartialHashesList;
class Transaction;

#include "../db/LevelDB.h"

class PendingTransactionsAgent : Agent {

public:
private:

public:


    class Hasher {
    public:
        std::size_t operator()(const ptr<partial_sha_hash>& a) const {
            size_t hash = 0;
            for (size_t i = 0; i < PARTIAL_SHA_HASH_LEN; i++) {
                hash = hash * 31 + (*a)[i];
            }
            return hash;
        };
    };



    class Equal {
    public:
        std::size_t operator() (const ptr<partial_sha_hash>& a, const ptr<partial_sha_hash>& b)  const {
            for (size_t i = 0; i < PARTIAL_SHA_HASH_LEN; i++) {
                if ((*a)[i] != (*b)[i]) {
                    return false;
                }
            }
            return true;
        };
    };


private:

    class Comparator {
    public:
        bool operator()(const ptr<partial_sha_hash> &a,
                        const ptr<partial_sha_hash>& b ) const {
            for (size_t i = 0; i < PARTIAL_SHA_HASH_LEN; i++) {
                if ((*a)[i] < (*b)[i])
                    return false;
                if ((*b)[i] < (*a)[i])
                    return true;
            }
            return false;
        }
    };

    unordered_set<ptr<partial_sha_hash>, Hasher, Equal> committedTransactions;
    unordered_map<ptr<partial_sha_hash>, ptr<Transaction> , Hasher, Equal> knownTransactions;


    transaction_count transactionCounter = 0;


    recursive_mutex transactionsMutex;


    shared_ptr<vector<ptr<Transaction>>> createTransactionsListForProposal();

public:

    PendingTransactionsAgent(Schain& _sChain);

    void pushKnownTransaction(ptr<Transaction> _transaction);

    uint64_t getKnownTransactionsSize();

    ptr<Transaction> getKnownTransactionByPartialHash(ptr<partial_sha_hash> hash);

    ptr<BlockProposal> buildBlockProposal(block_id _blockID, uint64_t  _previousBlockTimeStamp,
                             uint32_t _previosBlockTimeStampMs);


    virtual ~PendingTransactionsAgent() = default;


};



