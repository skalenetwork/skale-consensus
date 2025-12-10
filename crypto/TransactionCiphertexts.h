#pragma once

#include <memory>
#include <array>
#include <vector>
#include <cstdint>
#include "EncryptedAESKey.h"
#include <SkaleCommon.h>
#include <bite/BiteCiphertext.h>
#include "datastructures/SmallVector.h"

namespace libBLS {
    class CipheredKey;
}

/**
 * @brief Holds ciphertexts (EncryptedAESKeys) for a transaction. Transactions may have
 * 1 or multiple ciphertexts, depending on whether they are:
 * 
 * 1) A regular transaction - with a single EncryptedAESKey (single ciphertext)
 * 
 * 2) A CAT transaction - with multiple EncryptedAESKeys (multiple ciphertexts)
 */
class TransactionCiphertexts {
    
    small_vector<EncryptedAESKey> ciphertexts;

public:
    TransactionCiphertexts( ptr<BiteCiphertext> ciphertext);
    TransactionCiphertexts(std::vector<ptr<BiteCiphertext>>& ciphertextsVec);

    auto begin() const { return ciphertexts.begin(); }
    auto end()   const { return ciphertexts.end();   }
    auto cbegin() const { return ciphertexts.cbegin(); }
    auto cend()   const { return ciphertexts.cend();   }
    size_t count() const { return ciphertexts.size(); }

    EncryptedAESKeys& getCiphertexts() {
        return ciphertexts;
    }

    const EncryptedAESKey& operator[](size_t i) const {
        return ciphertexts[i];
    }

    EncryptedAESKey& operator[](size_t i) {
        return ciphertexts[i];
    }

    [[nodiscard]] std::shared_ptr<std::string> toHex() const;
};