//
// Created by kladko on 01.02.22.
//

#ifndef SKALED_TESTENCRYPTEDTRANSACTIONANALYZER_H
#define SKALED_TESTENCRYPTEDTRANSACTIONANALYZER_H


#include "node/ConsensusInterface.h"

class TestEncryptedTransactionAnalyzer : public EncryptedTransactionAnalyzer {

    ptr<vector<uint8_t>> teMagicStart;
    ptr<vector<uint8_t>> teMagicEnd;

public:
    TestEncryptedTransactionAnalyzer();

    shared_ptr<std::vector<uint8_t>> getLastSmartContractArgument(
            const std::vector<uint8_t> &_transaction) override;

};


#endif //SKALED_TESTENCRYPTEDTRANSACTIONANALYZER_H
