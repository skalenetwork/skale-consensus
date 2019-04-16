#pragma once

#include <boost/functional/hash.hpp>
#include "../Agent.h"


class Schain;
class BlockProposal;
class PartialHashesList;
class Transaction;

#include "../db/LevelDB.h"

class PendingTransactionsAgent : Agent, LevelDB::KeyVisitor {

public:
private:
    void visitDBKey(leveldb::Slice key) override;

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

    list<ptr<partial_sha_hash>> committedTransactionsList;

    transaction_count transactionCounter = 0;

    uint64_t committedTransactionCounter = 0;


    recursive_mutex transactionsMutex;


public:

    PendingTransactionsAgent(Schain& _sChain);

    uint64_t getCommittedTransactionCounter() const;

    transaction_count getTransactionCounter() const;

    void pushKnownTransaction(ptr<Transaction> _transaction);

    void pushKnownTransactions(ptr<vector<ptr<Transaction>>> _transactions);

    void waitUntilPendingTransaction();

    uint64_t getKnownTransactionsSize();

    uint64_t getPendingTransactionsSize();

    uint64_t getCommittedTransactionsSize();


    bool isCommitted(ptr< partial_sha_hash > _hash);

    void pushCommittedTransaction(shared_ptr<Transaction> t);


    shared_ptr<vector<ptr<Transaction>>> createTransactionsListForProposal();

    ptr<Transaction> getKnownTransactionByPartialHash(ptr<partial_sha_hash> hash);

    ptr<BlockProposal> buildBlockProposal(block_id _blockID, uint64_t  _previousBlockTimeStamp);


    void addToCommitted(shared_ptr<partial_sha_hash> &s);


};



