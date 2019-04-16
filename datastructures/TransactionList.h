#pragma once

#include "DataStructure.h"


class Transaction;

class TransactionList : public DataStructure  {

    ptr<vector<uint8_t>> serializedTransactions = nullptr;

    ptr<vector<ptr<Transaction>>> transactions = nullptr;

public:


    TransactionList(ptr<vector<size_t>> transactionSizes_, ptr<vector<uint8_t>> serializedTransactions, uint32_t  offset = 0);

    TransactionList(ptr<vector<ptr<Transaction>>> _transactions);

    ptr<vector<ptr<Transaction>>> getItems() ;

    shared_ptr<vector<uint8_t>> serialize() ;

    size_t size();


    static atomic<uint64_t>  totalObjects;

    static uint64_t getTotalObjects() {
        return totalObjects;
    }

    virtual ~TransactionList();


};



