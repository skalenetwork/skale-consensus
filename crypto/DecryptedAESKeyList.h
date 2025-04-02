#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#include <boost/container/flat_map.hpp>
#pragma GCC diagnostic pop
#include "DecryptedAESKey.h"

class DecryptedAESKeyList {
public:
    // Optional: Constructor
    DecryptedAESKeyList() = default;

    // Optional: Add public access methods
    void addKey(transaction_index _index, const DecryptedAESKey& key) {
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
    boost::container::flat_map<transaction_index, DecryptedAESKey> decryptedAESKeys;
    std::atomic<bool> isComplete = false;
};

