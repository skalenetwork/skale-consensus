#pragma once


#include "libBLS/threshold_encryption/TEDecryptionShare.h"

#include "AESKeyDecryptionShare.h"

namespace libff {
    class alt_bn128_G2;
}

namespace libBLS {
    class TEDecryptionShare;
}


class ConsensusAESKeyDecryptionShare : public AESKeyDecryptionShare {
    ptr<libBLS::TEDecryptionShare> aesKeyDecryptionShare;

public:
    ConsensusAESKeyDecryptionShare(
        const ptr<libBLS::TEDecryptionShare> &_decryptionShare, block_id _blockID,
        transaction_index _transactionIndex, schain_index _decryptorIndex, bool _decryptionFailed);


    ConsensusAESKeyDecryptionShare(const string &_decryptionShare,
                                   block_id _blockID, transaction_index _transactionIndex, schain_index _decryptorIndex,
                                   bool _decryptionFailed, uint64_t _totalDecryptors,
                                   uint64_t _requiredDecryptors);


    [[nodiscard]] ptr<libBLS::TEDecryptionShare> getTEDecryptionShare() const;

    string toString() override;

    ~ConsensusAESKeyDecryptionShare() override;
};
