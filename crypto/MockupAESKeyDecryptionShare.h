#pragma once


#include "ThresholdAESKeyDecryptionShare.h"


class MockupAESKeyDecryptionShare: public ThresholdAESKeyDecryptionShare {
    uint64_t totalDecryptors = 0;
    uint64_t requiredDecryptors = 0;
    string aesDecryptionShare;

public:
    MockupAESKeyDecryptionShare( const string& _aesKeyDecryptionShare, schain_id _schainID, block_id _blockID,
        transaction_index _transactionIndex,
        schain_index _decryptorIndex, size_t _totalDecryptors, size_t _requiredDecryptors );

    string toString() override;

    ~MockupAESKeyDecryptionShare() override;
};

