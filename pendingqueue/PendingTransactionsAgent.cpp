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

    @file PendingTransactionsAgent.cpp
    @author Stan Kladko
    @date 2018
*/

#include <unordered_set>
#include "../SkaleCommon.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../thirdparty/json.hpp"
#include "leveldb/db.h"
#include "../db/LevelDB.h"
#include "../db/BlockDB.h"

#include "../utils/Time.h"
#include "../crypto/SHAHash.h"
#include "../crypto/CryptoManager.h"
#include "../datastructures/BlockProposal.h"
#include "../datastructures/MyBlockProposal.h"
#include "../node/Node.h"
#include "../datastructures/PartialHashesList.h"
#include "../datastructures/Transaction.h"
#include "../datastructures/TransactionList.h"
#include "../pendingqueue/TestMessageGeneratorAgent.h"
#include "../chains/Schain.h"
#include "../node/ConsensusEngine.h"
#include "PendingTransactionsAgent.h"
#include "../db/CommittedTransactionDB.h"

#include "../microprofile.h"

using namespace std;


PendingTransactionsAgent::PendingTransactionsAgent( Schain& ref_sChain )
    : Agent(ref_sChain, false)  {}

ptr<BlockProposal> PendingTransactionsAgent::buildBlockProposal(block_id _blockID, uint64_t _previousBlockTimeStamp,
    uint32_t _previousBlockTimeStampMs) {

    MICROPROFILE_ENTERI( "PendingTransactionsAgent", "sleep", MP_DIMGRAY );
    usleep(getNode()->getMinBlockIntervalMs() * 1000);
    MICROPROFILE_LEAVE();

    shared_ptr<vector<ptr<Transaction>>> transactions = createTransactionsListForProposal();


    while (Time::getCurrentTimeMs() <= _previousBlockTimeStamp * 1000 + _previousBlockTimeStampMs) {
        usleep(10);
    }


    auto transactionList = make_shared<TransactionList>(transactions);

    auto currentTime = Time::getCurrentTimeMs();
    auto sec = currentTime / 1000;
    auto m = (uint32_t) (currentTime % 1000);

    auto myBlockProposal = make_shared<MyBlockProposal>(*sChain, _blockID, sChain->getSchainIndex(),
            transactionList, sec, m, getSchain()->getCryptoManager());

    LOG(trace, "Created proposal, transactions:" + to_string(transactions->size()));

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

        if((uint64_t ) diff.total_milliseconds() >= getSchain()->getNode()->getEmptyBlockIntervalMs())
            break;

        if (sChain->getExtFace()) {
            tx_vec = sChain->getExtFace()->pendingTransactions(need_max);
        } else {
            tx_vec = sChain->getTestMessageGeneratorAgent()->pendingTransactions(need_max);
        }
    }

    for(const auto& e: tx_vec){
        ptr<Transaction> pt = Transaction::deserialize( make_shared<std::vector<uint8_t>>(e),
                0,e.size(), false );
        result->push_back(pt);
        pushKnownTransaction(pt);
    }

    return result;
}


ptr<Transaction> PendingTransactionsAgent::getKnownTransactionByPartialHash(ptr<partial_sha_hash> hash) {
    lock_guard<recursive_mutex> lock(transactionsMutex);
    if (knownTransactions.count(hash))
        return knownTransactions.at(hash);
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


uint64_t PendingTransactionsAgent::getKnownTransactionsSize() {
    lock_guard<recursive_mutex> lock(transactionsMutex);
    return knownTransactions.size();
}






