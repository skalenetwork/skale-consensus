
#include "SkaleCommon.h"
#include "Log.h"
#include "crypto/EncryptedAESKey.h"
#include "crypto/DecryptedAESKey.h"
#include "node/ConsensusInterface.h"
#include "MockupTEPublicKey.h"



ptr<EncryptedAESKey> MockupTEPublicKey::encryptAESKey(ptr<DecryptedAESKey>& _decryptedAESKey) {
    CHECK_STATE(_decryptedAESKey);
    auto encryptedKeyBytes = make_shared<array<std::uint8_t, BITE_ENCRYPTED_AES_KEY_LEN>>();
    encryptedKeyBytes->at(0) = 1;;
    for (size_t i = 0; i < BITE_AES_KEY_LEN; i++) {
        encryptedKeyBytes->at(i + 1) = _decryptedAESKey->at(i);
    }
    auto encryptedAESKey = make_shared<EncryptedAESKey>(encryptedKeyBytes);
    return encryptedAESKey;
}


