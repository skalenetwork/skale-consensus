//
// Created by kladko on 01.02.22.
//

#include "SkaleCommon.h"
#include "Log.h"

#include "network/Utils.h"
#include "datastructures/BlockEncryptedArguments.h"
#include "BlockDecryptedAesKeys.h"


BlockDecryptedAesKeys::BlockDecryptedAesKeys() {
    decryptedAesKeys = make_shared<map<uint64_t, ptr<vector<uint8_t>>>>();
}

BlockDecryptedAesKeys::BlockDecryptedAesKeys(ptr<map<uint64_t, ptr<vector<uint8_t>>>> decryptedAesKeys)
        : decryptedAesKeys(decryptedAesKeys) {}

BlockDecryptedAesKeys::BlockDecryptedAesKeys(ptr<map<uint64_t, string>> _decryptedAesKeys) {
    CHECK_STATE(_decryptedAesKeys);
    decryptedAesKeys = make_shared<map<uint64_t, ptr<vector<uint8_t>>>>();
    for (auto&& item : *_decryptedAesKeys) {
        CHECK_STATE(item.second.size() == AES_KEY_LEN_BYTES * 2);
        auto rawKey = make_shared<vector<uint8_t>>(AES_KEY_LEN_BYTES);
        Utils::cArrayFromHex(item.second, rawKey->data(), AES_KEY_LEN_BYTES);
        CHECK_STATE(decryptedAesKeys->emplace(item.first, rawKey).second);
    }
}

ptr<map<uint64_t, ptr<vector<uint8_t>>>> BlockDecryptedAesKeys::decryptArgs(ptr<BlockEncryptedArguments> _encryptedArgs) {
    CHECK_STATE(_encryptedArgs);
    CHECK_STATE(decryptedAesKeys->size() == _encryptedArgs->size())

    auto result = make_shared<map<uint64_t, ptr<vector<uint8_t>>>>();

    for (auto&& item : *_encryptedArgs->getAesEncryptedSegments()) {
        auto key = decryptedAesKeys->at(item.first);
    }

}

ptr<map<uint64_t, string>> BlockDecryptedAesKeys::getDecryptedAesKeysAsHex()  {

    auto result = make_shared<map<uint64_t, string>>();

    for (auto&& item: *decryptedAesKeys) {
        auto binaryKey = item.second;
        CHECK_STATE(binaryKey->size() == AES_KEY_LEN_BYTES);
        auto hexKey = Utils::vector2Hex(binaryKey);
        CHECK_STATE(result->emplace(item.first, hexKey).second);
    }

    return result;
}



