#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#include <boost/container/flat_map.hpp>
#include "datastructures/SmallVector.h"
#pragma GCC diagnostic pop
#include "DecryptedAESKey.h"

class DecryptedAESKeyList {
public:

    [[nodiscard]] boost::container::flat_map<transaction_index, ptr<DecryptedAESKeys>>& getKeys();

    // Optional: Constructor
    DecryptedAESKeyList() = default;


    // Optional: Add public access methods
    void addKeys(transaction_index _index, const DecryptedAESKeys& keys) {
        decryptedAESKeys.emplace(_index, make_shared<DecryptedAESKeys>(keys));
        totalDecryptedCiphertexts += keys.size();
    }

    const  ptr<DecryptedAESKeys> getKeys(transaction_index _transactionIndex) const {
        auto result = decryptedAESKeys.find(_transactionIndex);
        if (result == decryptedAESKeys.end()) {
            return nullptr;
        }
        return result->second;
    }

    uint64_t getSize() const {
        return decryptedAESKeys.size();
    }

    size_t totalDecryptedCiphertextsCount() const {
        return totalDecryptedCiphertexts;
    }


private:
    boost::container::flat_map<transaction_index, ptr<DecryptedAESKeys>> decryptedAESKeys;
    size_t totalDecryptedCiphertexts;
};

