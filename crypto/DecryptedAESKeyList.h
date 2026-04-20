#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#include <boost/container/flat_map.hpp>
#pragma GCC diagnostic pop
#include "DecryptedAESKey.h"

class DecryptedAESKeyList {
public:

    [[nodiscard]] boost::container::flat_map<transaction_index, ptr<DecryptedAESKey>>& getKeys();

    // Optional: Constructor
    DecryptedAESKeyList() = default;


    // Optional: Add public access methods
    void addKey(transaction_index _index, const DecryptedAESKey& key) {
        decryptedAESKeys.emplace(_index, make_shared<DecryptedAESKey>(key));
    }

    const  ptr<DecryptedAESKey> getKey(transaction_index _transactionIndex) const {
        auto result = decryptedAESKeys.find(_transactionIndex);
        if (result == decryptedAESKeys.end()) {
            return nullptr;
        }
        return result->second;
    }

    uint64_t getSize() const {
        return decryptedAESKeys.size();
    }


private:
    boost::container::flat_map<transaction_index, ptr<DecryptedAESKey>> decryptedAESKeys;
};

