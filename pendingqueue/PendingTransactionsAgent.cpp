#include <unordered_set>
#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../thirdparty/json.hpp"
#include "leveldb/db.h"
#include "../db/LevelDB.h"
#include "../crypto/SHAHash.h"
#include "../datastructures/BlockProposal.h"
#include "../datastructures/MyBlockProposal.h"
#include "../node/Node.h"
#include "../datastructures/PartialHashesList.h"
#include "../datastructures/PendingTransaction.h"
#include "../datastructures/TransactionList.h"

#include "../chains/Schain.h"

#include "../node/ConsensusEngine.h"

#include "PendingTransactionsAgent.h"

#include "leveldb/db.h"


using namespace std;


PendingTransactionsAgent::PendingTransactionsAgent( Schain& ref_sChain )
    : Agent(ref_sChain, false)  {


    auto cfg = getSchain()->getNode()->getCfg();


    auto db = getSchain()->getNode()->getBlocksDB();

    static string count("COUNT");

    auto value = db->readString(count);

    if (value != nullptr) {
        committedTransactionCounter = stoul(*value);
    }

    auto cdb = getNode()->getCommittedTransactionsDB();


    cdb->visitKeys(this, getNode()->getCommittedTransactionHistoryLimit() - committedTransactionCounter);

}

void PendingTransactionsAgent::visitDBKey(leveldb::Slice _key) {
    auto s = make_shared<partial_sha_hash>();
    std::copy_n((uint8_t*)_key.data(), PARTIAL_SHA_HASH_LEN, s->begin());
    addToCommitted(s);
    committedTransactionCounter++;
}



void PendingTransactionsAgent::addToCommitted(ptr<partial_sha_hash> &s)  {
    committedTransactions.insert(s);
    committedTransactionsList.push_back(s);
}

ptr<BlockProposal> PendingTransactionsAgent::buildBlockProposal(block_id _blockID, uint64_t _previousBlockTimeStamp) {

    usleep(getNode()->getMinBlockIntervalMs() * 1000);

    shared_ptr<vector<ptr<Transaction>>> transactions = createTransactionsListForProposal();


    while ((uint64_t )Schain::getCurrentTimeSec() <= _previousBlockTimeStamp) {
        usleep(1000);
    }


    auto transactionList = make_shared<TransactionList>(transactions);

    auto myBlockProposal = make_shared<MyBlockProposal>(*sChain, _blockID, sChain->getSchainIndex(),
            transactionList, Schain::getCurrentTimeSec());
    LOG(trace, "Created proposal, transactions:" + to_string(transactions->size()));
;
    transactionCounter += (uint64_t) myBlockProposal->createPartialHashesList()->getTransactionCount();
    return myBlockProposal;
}

shared_ptr<vector<ptr<Transaction>>> PendingTransactionsAgent::createTransactionsListForProposal() {
    auto result = make_shared<vector<ptr<Transaction>>>();

    size_t need_max = getNode()->getMaxTransactionsPerBlock();
    ConsensusExtFace::transactions_vector tx_vec;

    boost::posix_time::ptime t1 = boost::posix_time::microsec_clock::local_time();

    while(tx_vec.empty()){

        getSchain()->getNode()->exitCheck();
        boost::posix_time::ptime t2 = boost::posix_time::microsec_clock::local_time();
        boost::posix_time::time_duration diff = t2 - t1;
        if(diff.total_milliseconds() >= getSchain()->getNode()->getEmptyBlockIntervalMs())
            break;

        tx_vec = sChain->getExtFace()->pendingTransactions(need_max);
    }

    for(const auto& e: tx_vec){
        ptr<Transaction> pt = make_shared<PendingTransaction>( make_shared<std::vector<uint8_t>>(e) );
        result->push_back(pt);
        pushKnownTransaction(pt);
    }

    return result;
}


void PendingTransactionsAgent::pushKnownTransactions(ptr<vector<ptr<Transaction>>> _transactions) {
    for (auto &&t: *_transactions) {
        pushKnownTransaction(t);
    }
}

ptr<Transaction> PendingTransactionsAgent::getKnownTransactionByPartialHash(ptr<partial_sha_hash> hash) {
    lock_guard<recursive_mutex> lock(transactionsMutex);
    if (knownTransactions.count(hash))
        return knownTransactions[hash];
    return nullptr;
}

void PendingTransactionsAgent::pushKnownTransaction(ptr<Transaction> _transaction) {
    lock_guard<recursive_mutex> lock(transactionsMutex);
    if (knownTransactions.count(_transaction->getPartialHash())) {
        LOG(trace, "Duplicate transaction pushed to known transactions");
        return;
    }
    knownTransactions[_transaction->getPartialHash()] = _transaction;


    while (knownTransactions.size() > KNOWN_TRANSACTIONS_HISTORY) {
        auto t1 = knownTransactions.begin()->first;
        knownTransactions.erase(t1);
    }
}


transaction_count PendingTransactionsAgent::getTransactionCounter() const {
    return transactionCounter;
}

uint64_t PendingTransactionsAgent::getKnownTransactionsSize() {
    lock_guard<recursive_mutex> lock(transactionsMutex);
    return knownTransactions.size();
}

uint64_t PendingTransactionsAgent::getCommittedTransactionsSize() {
    lock_guard<recursive_mutex> lock(transactionsMutex);
    return committedTransactions.size();
}

bool PendingTransactionsAgent::isCommitted(ptr<partial_sha_hash> _hash) {
    lock_guard<recursive_mutex> lock(transactionsMutex);
   auto isCommitted = committedTransactions.count(_hash ) > 0;
        return isCommitted;
}


void PendingTransactionsAgent::pushCommittedTransaction(shared_ptr<Transaction> t) {
    lock_guard<recursive_mutex> lock(transactionsMutex);
    committedTransactions.insert(t->getPartialHash());
    committedTransactionsList.push_back(t->getPartialHash());
    while (committedTransactions.size() > getNode()->getCommittedTransactionHistoryLimit()) {
        committedTransactions.erase(committedTransactionsList.front());
        committedTransactionsList.pop_front();
    }

    ASSERT(committedTransactionsList.size() >= committedTransactions.size());

    auto db = getNode()->getCommittedTransactionsDB();


    auto key = (const char *) t->getPartialHash()->data();
    auto keyLen = PARTIAL_SHA_HASH_LEN;
    auto value = (const char*) &committedTransactionCounter;
    auto valueLen = sizeof(committedTransactionCounter);


    db->writeByteArray(key, keyLen, value, valueLen);

    static auto key1 = string("transactions");
    auto value1 = to_string(committedTransactionCounter);

    db->writeString(key1, value1);

    committedTransactionCounter++;


}

uint64_t PendingTransactionsAgent::getCommittedTransactionCounter() const {
    return committedTransactionCounter;
}


