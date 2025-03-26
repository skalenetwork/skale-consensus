
#include "SkaleCommon.h"
#include "Log.h"
#include "crypto/EncryptedAESKey.h"
#include "crypto/DecryptedAESKey.h"
#include "node/ConsensusInterface.h"
#include "MockupTEPublicKey.h"



ptr<EncryptedAESKey> MockupTEPublicKey::encryptAESKey(ptr<DecryptedAESKey>& _decryptedAESKey) {
    CHECK_STATE(_decryptedAESKey);
    auto encryptedKeyBytes = make_shared<std::array<std::uint8_t, BITE_ENCRYPTED_AES_KEY_LEN>>();
    auto randomBytes = make_shared<std::array<std::uint8_t, BITE_TE_RANDOM_LEN>>();
    std::iota(randomBytes->begin(), randomBytes->end(), 0);

    encryptedKeyBytes->at(0) = 1;

    std::copy(_decryptedAESKey->getAesKey(), _decryptedAESKey->getAesKey(), encryptedKeyBytes->begin() + 1);
    std::copy(randomBytes->begin(), randomBytes->end(), encryptedKeyBytes->begin() + BITE_AES_KEY_LEN + 1);

    return make_shared<EncryptedAESKey>(encryptedKeyBytes);
}

