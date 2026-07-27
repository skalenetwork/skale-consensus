#pragma once

#include "bite/crypto/CryptographicValidationMode.h"
#include "libBLS/threshold_encryption/TEDecryptionShare.h"
#include "AESKeyDecryptionShare.h"

namespace libBLS {
    class TEDecryptionShare;
}

class ConsensusAESKeyDecryptionShare : public AESKeyDecryptionShare {
    ptr<libBLS::TEDecryptionShare> aesKeyDecryptionShare;

public:
    ConsensusAESKeyDecryptionShare(
        const ptr<libBLS::TEDecryptionShare> &_decryptionShare, schain_index _decryptorIndex, bool _decryptionFailed);


    ConsensusAESKeyDecryptionShare(const string &_decryptionShare,
                                  schain_index _decryptorIndex,
                                   bool _decryptionFailed,
                                   CryptographicValidationMode _validationMode = CryptographicValidationMode::Validate);


    [[nodiscard]] ptr<libBLS::TEDecryptionShare> getTEDecryptionShare() const;

    string toString() override;

    ~ConsensusAESKeyDecryptionShare() override;
};
