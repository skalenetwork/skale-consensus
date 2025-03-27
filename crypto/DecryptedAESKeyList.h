#pragma once
#include <boost/container/flat_map.hpp>
#include "DecryptedAESKey.h"

class DecryptedAESKeyList {
public:
    // Optional: Constructor
    DecryptedAESKeyList() = default;

    // Optional: Add public access methods
    void addKey(uint64_t _index, const DecryptedAESKey& key) {
        decryptedAESKeys.emplace(_index, key);
    }

    const DecryptedAESKey* getKey(uint64_t id) const {
        CHECK_STATE(isComplete);
        auto it = decryptedAESKeys.find(id);
        return (it != decryptedAESKeys.end()) ? &it->second : nullptr;
    }

    void markComplete() {
        isComplete = true;
    }


private:
    boost::container::flat_map<uint64_t, DecryptedAESKey> decryptedAESKeys;
    std::atomic<bool> isComplete = false;
};

