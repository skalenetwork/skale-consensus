//
// Created by stan on 19.03.18.
//

#include "../SkaleConfig.h"
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
