#include <random>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include "Log.h"
#include "network/Utils.h"
#include "node/ConsensusInterface.h"
#include "EncryptedAESKey.h"


EncryptedAESKey::EncryptedAESKey(std::array<std::uint8_t, BITE_ENCRYPTED_AES_KEY_LEN> _key)
    : regularTxKey(_key) {
}


ptr<string> EncryptedAESKey::toHex() const {
    return  make_shared<string>(Utils::carray2Hex(regularTxKey.data(), regularTxKey.size()));
 }

