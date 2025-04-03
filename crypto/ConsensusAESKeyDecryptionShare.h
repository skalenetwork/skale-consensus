#pragma once


#include "libBLS/threshold_encryption/TEDecryptionShare.h"

#include "ThresholdAESKeyDecryptionShare.h"

namespace libff {
class alt_bn128_G2;
}

namespace libBLS {
    class TEDecryptionShare;
}


class ConsensusAESKeyDecryptionShare : public ThresholdAESKeyDecryptionShare {
    ptr<libBLS::TEDecryptionShare > aesKeyDecryptionShare;

public:


    ConsensusAESKeyDecryptionShare(
        const ptr< libBLS::TEDecryptionShare >& _decryptionShare, schain_id _schainId, block_id _blockID,
        transaction_index _transactionIndex);


    ConsensusAESKeyDecryptionShare( const string& _decryptionShare, schain_id _schainID, block_id _blockID,
        transaction_index _transactionIndex, schain_index _decryptorIndex, uint64_t _totalDecryptors, uint64_t _requiredDecryptors );


    [[nodiscard]] ptr< libBLS::TEDecryptionShare > getTEDecryptionShare() const;

    string toString() override;

    ~ConsensusAESKeyDecryptionShare() override;
};


