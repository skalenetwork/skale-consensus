#pragma once

#include <memory>
#include <array>
#include <vector>
#include <cstdint>
#include <optional>
#include "crypto/EncryptedAESKey.h"
#include <SkaleCommon.h>
#include "node/ConsensusTypes.h"
#include "bite/BiteCiphertext.h"
#include "datastructures/SmallVector.h"

namespace libBLS {
    class CipheredKey;
}

/**
 * @brief Holds ciphertexts (EncryptedAESKeys) for a transaction. Transactions may have
 * 0 or multiple ciphertexts, depending on whether they are:
 *
 * 1) A regular transaction - Requires a single EncryptedAESKey (single ciphertext)
 *
 * 2) A CAT transaction - May have 0 or multiple EncryptedAESKeys (multiple ciphertexts)
 */
class TransactionCiphertexts {

    // Ciphertexts for the transaction
    small_vector<EncryptedAESKey> ciphertexts;

    // Optional smart contract address used as AAD in threshold encryption
    // For CAT transactions, this is the address of the SC that issued
    // the CAT.
    std::optional<AddressBytes> scAddressAadTE;

public:
    TransactionCiphertexts( ptr<BiteCiphertext> ciphertext,
        std::optional<AddressBytes> scAddressAadTE = std::nullopt);

    TransactionCiphertexts(std::vector<ptr<BiteCiphertext>>& ciphertextsVec,
        std::optional<AddressBytes> scAddressAadTE = std::nullopt);

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

    // Returns the SC address AAD for TE validation (only set for CAT txs)
    [[nodiscard]] const std::optional<AddressBytes>& getScAddressAadTE() const { return scAddressAadTE; }

    // Returns true if this is a CAT transaction (has SC address AAD)
    [[nodiscard]] bool isCTX() const { return scAddressAadTE.has_value(); }
};
