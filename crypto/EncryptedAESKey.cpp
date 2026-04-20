#include <random>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include "Log.h"
#include "network/Utils.h"
#include "node/ConsensusInterface.h"
#include "EncryptedAESKey.h"


EncryptedAESKey::EncryptedAESKey(std::shared_ptr<std::array<std::uint8_t, BITE_ENCRYPTED_AES_KEY_LEN>> key)
    : key(key) {
    CHECK_STATE(key);
}


ptr<string> EncryptedAESKey::toHex() const {
    CHECK_STATE(key);
    return  make_shared<string>(Utils::carray2Hex(key->data(), key->size()));
 }

