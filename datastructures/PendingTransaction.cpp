#include "../SkaleConfig.h"
#include "PendingTransaction.h"


PendingTransaction::PendingTransaction(const ptr<vector<uint8_t>> data) : Transaction(data) {
    totalObjects++;
}



PendingTransaction::~PendingTransaction() {
    totalObjects--;
}


atomic<uint64_t>  PendingTransaction::totalObjects(0);
