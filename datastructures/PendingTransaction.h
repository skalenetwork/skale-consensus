#pragma once


#include "Transaction.h"

class PendingTransaction  : public Transaction{
public:
    PendingTransaction(const ptr<vector<uint8_t>> data);



    static atomic<uint64_t>  totalObjects;

    static uint64_t getTotalObjects() {
        return totalObjects;
    };

    virtual ~PendingTransaction();

};