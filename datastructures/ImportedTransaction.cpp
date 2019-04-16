#include "../SkaleConfig.h"
#include "ImportedTransaction.h"


ImportedTransaction::ImportedTransaction(const ptr<vector<uint8_t>> data) : Transaction(data) {
    totalObjects++;
}



ImportedTransaction::~ImportedTransaction() {
    totalObjects--;
}


atomic<uint64_t>  ImportedTransaction::totalObjects(0);
