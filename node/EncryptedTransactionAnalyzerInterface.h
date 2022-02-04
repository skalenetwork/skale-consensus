//
// Created by kladko on 04.02.22.
//

#ifndef SKALED_ENCRYPTEDTRANSACTIONANALYZERINTERFACE_H
#define SKALED_ENCRYPTEDTRANSACTIONANALYZERINTERFACE_H

class EncryptedTransactionAnalyzerInterface {

public:

    virtual shared_ptr<std::vector<uint8_t>> getEncryptedData(const std::vector<uint8_t> &_transaction) = 0;

};

#endif //SKALED_ENCRYPTEDTRANSACTIONANALYZERINTERFACE_H
