#pragma once


#include "EncryptedAESKey.h"
#include "AESKeyDecryptionShare.h"


class MockupAESKeyDecryptionShare: public AESKeyDecryptionShare {
    uint64_t totalDecryptors = 0;
    uint64_t requiredDecryptors = 0;
    string aesDecryptionShare;

public:
    MockupAESKeyDecryptionShare( const string _aesKeyDecryptionShare, block_id _blockID,
        transaction_index _transactionIndex,
        schain_index _decryptorIndex,bool _decryptionFailed, size_t _totalDecryptors, size_t _requiredDecryptors) ;

    string toString() override;

    ~MockupAESKeyDecryptionShare() override;

    static ptr<MockupAESKeyDecryptionShare> mockupDecrypt(ptr <EncryptedAESKey> _key);
};

