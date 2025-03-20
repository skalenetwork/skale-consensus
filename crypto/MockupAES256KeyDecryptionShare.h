#pragma once


#include "ThresholdAES256KeyDecryptionShare.h"


class MockupAES256KeyDecryptionShare: public ThresholdAES256KeyDecryptionShare {
    uint64_t totalDecryptors = 0;
    uint64_t requiredDecryptors = 0;
    string aes256DecryptionShare;

public:
    MockupAES256KeyDecryptionShare( const string& _aes256KeyDecryptionShare, schain_id _schainID, block_id _blockID,
        transaction_index _transactionIndex,
        schain_index _decryptorIndex, size_t _totalDecryptors, size_t _requiredDecryptors );

    string toString() override;

    ~MockupAES256KeyDecryptionShare() override;
};

