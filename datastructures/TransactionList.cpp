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

    @file TransactionList.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../SkaleCommon.h"
#include "../Agent.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "Transaction.h"
#include "ImportedTransaction.h"

#include "TransactionList.h"


TransactionList::TransactionList(ptr<vector<ptr<Transaction>>> _transactions) {

    ASSERT(_transactions);
//    ASSERT(_transactions->size() > 0);
    totalObjects++;

    transactions = _transactions;

}


TransactionList::TransactionList(ptr<vector<size_t>> transactionSizes_,
                                                       ptr<vector<uint8_t>> serializedTransactions, uint32_t _offset) {

    totalObjects++;
    size_t index = _offset;

    transactions = make_shared<vector<ptr<Transaction>>>();
    transactions->reserve(transactionSizes_->size());

    for (auto &&size : *transactionSizes_) {
        auto endIndex = index + size;
        ASSERT(index + size <= serializedTransactions->size());
        auto transactionData = make_shared<vector<uint8_t>>(serializedTransactions->begin() + index, serializedTransactions->begin() + endIndex);
        auto transaction = make_shared<ImportedTransaction>(transactionData);
        transactions->push_back(transaction);
        index = endIndex;
    }


};


ptr<vector<ptr<Transaction>>> TransactionList::getItems() {
    return transactions;
}

shared_ptr<vector<uint8_t>>TransactionList::serialize()  {

    if (serializedTransactions)
        return serializedTransactions;


    size_t totalSize = 0;

    for (auto &&transaction : *transactions) {
        totalSize += transaction->getData()->size();
    }

    for (auto &&transaction : *transactions) {
        totalSize += transaction->getData()->size();
    }


    serializedTransactions = make_shared<vector<uint8_t>>();

    serializedTransactions->reserve(totalSize);


    for (auto &&transaction : *transactions) {
        auto data = transaction->getData();
        serializedTransactions->insert(serializedTransactions->end(), data->begin(), data->end());
    }
    return serializedTransactions;
}

TransactionList::~TransactionList() {
    totalObjects--;
}



atomic<uint64_t>  TransactionList::totalObjects(0);

size_t TransactionList::size() {
    return transactions->size();
}
