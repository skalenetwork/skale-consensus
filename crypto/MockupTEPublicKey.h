#pragma once

#include "EncryptedAESKey.h"

class DecryptedAESKey;

class MockupTEPublicKey {

public:
    explicit MockupTEPublicKey() {}


    ptr<EncryptedAESKey> encryptAESKey(ptr<DecryptedAESKey> &_decryptedAESKey);
};
