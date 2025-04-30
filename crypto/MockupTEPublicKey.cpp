
#include "SkaleCommon.h"
#include "Log.h"
#include "crypto/EncryptedAESKey.h"
#include "crypto/DecryptedAESKey.h"
#include "node/ConsensusInterface.h"
#include "MockupTEPublicKey.h"



ptr<EncryptedAESKey> MockupTEPublicKey::encryptAESKey(ptr<DecryptedAESKey>& _decryptedAESKey) {
    CHECK_STATE(_decryptedAESKey);

    // Allocate encrypted key bytes and random bytes
    auto encryptedKeyBytes = std::make_shared<std::array<std::uint8_t, BITE_ENCRYPTED_AES_KEY_LEN>>();
    auto randomBytes = std::make_shared<std::array<std::uint8_t, BITE_TE_RANDOM_LEN>>();

    std::iota(randomBytes->begin(), randomBytes->end(), 0);

    encryptedKeyBytes->at(0) = 1;

    // Copy AES key into encryptedKeyBytes starting from index 1
    std::copy_n(
        _decryptedAESKey->getAesKey().begin(),
        BITE_AES_KEY_LEN,
        encryptedKeyBytes->begin() + 1
    );

    // Copy random bytes after the AES key in the encryptedKeyBytes
    std::copy(
        randomBytes->begin(),
        randomBytes->end(),
        encryptedKeyBytes->begin() + 1 + BITE_AES_KEY_LEN
    );

    return std::make_shared<EncryptedAESKey>(encryptedKeyBytes);
}
