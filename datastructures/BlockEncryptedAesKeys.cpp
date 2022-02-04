//
// Created by kladko on 02.02.22.
//

#include "SkaleCommon.h"
#include "Log.h"

#include "BlockEncryptedAesKeys.h"

BlockEncryptedAesKeys::BlockEncryptedAesKeys() {
    encryptedKeys = make_shared<map<uint64_t, string>>();
}

void BlockEncryptedAesKeys::add(uint64_t _transactionIndex, const string &_key) {
    LOCK(m)
    CHECK_STATE(_key.size() >= AES_KEY_LEN_BYTES);
    CHECK_STATE(encryptedKeys->emplace(_transactionIndex, _key).second);
}

uint64_t BlockEncryptedAesKeys::size() {
    LOCK(m)
    return encryptedKeys->size();
}
