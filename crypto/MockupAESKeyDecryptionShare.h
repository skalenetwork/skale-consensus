#pragma once


#include "EncryptedAESKey.h"
#include "AESKeyDecryptionShare.h"


class MockupAESKeyDecryptionShare: public AESKeyDecryptionShare {
    string aesDecryptionShare;

public:
    MockupAESKeyDecryptionShare( const string _aesKeyDecryptionShare,
        schain_index _decryptorIndex,bool _decryptionFailed) ;

    string toString() override;

    ~MockupAESKeyDecryptionShare() override;

    static ptr<MockupAESKeyDecryptionShare> mockupDecrypt(ptr <EncryptedAESKey> _key, schain_index _decryptorIndex);
};

