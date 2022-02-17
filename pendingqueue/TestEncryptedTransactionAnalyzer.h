//
// Created by kladko on 01.02.22.
//

#ifndef SKALED_TESTENCRYPTEDTRANSACTIONANALYZER_H
#define SKALED_TESTENCRYPTEDTRANSACTIONANALYZER_H

#include "node/EncryptedTransactionAnalyzerInterface.h"

class TestEncryptedTransactionAnalyzer : public EncryptedTransactionAnalyzerInterface {

    ptr<vector<uint8_t>> teMagicStart;
    ptr<vector<uint8_t>> teMagicEnd;

public:
    TestEncryptedTransactionAnalyzer();

    shared_ptr<std::vector<uint8_t>> getEncryptedData(
            const std::vector<uint8_t> &_transaction);

};


#endif //SKALED_TESTENCRYPTEDTRANSACTIONANALYZER_H
