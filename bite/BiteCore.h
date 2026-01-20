#pragma once
#include <vector>
#include <optional>
#include <string>
#include "SkaleCommon.h"
#include <cstdint>
#include "libBLS/threshold_encryption/threshold_encryption.h"

namespace libBLS {
    class TEPublicKey;
} // namespace libBLS

/**
 * @brief Core BITE encryption/decryption functionality.
 * Represents a detached module that does not depend on other parts of the system (such as BiteManager, Consensus, etc).
 * Handles low-level encryption/decryption and validation operations.
 * It can be used both in real crypto mode and in mockup mode (for testing and local development).
 */
struct BiteCore {
    bool doRealCrypto = true;

public:
    struct CiphertextValidationResult {
        bool allValid;

        std::vector<bool> validationResults; // per-ciphertext validation results
        std::vector<std::string> publicDecryptionValues;
    };

    std::vector<uint8_t> encryptData( const libBLS::TEPublicKey& _key, 
        const std::vector<uint8_t>& _plainData, const std::optional<std::vector<uint8_t>>& _aadTE = std::nullopt ) const;
    
    std::vector<uint8_t> decryptData( const libBLS::AES256Key& _aesKey, 
        std::vector<uint8_t>& _cipherData, bool validate = true ) const;

    /**
     * @brief Validates a batch of ciphertexts.
     * If all are valid, also prepares public decryption values for them.
     * If any is invalid, then the invalid index is marked in validationResults as false,
     * and publicDecryptionValues is left empty.
     * @param _aadTE Optional AAD vector aligned with ciphertexts (nullptr = no AAD for all)
     */
    CiphertextValidationResult validateCiphertexts(
        const std::vector<libBLS::CipheredKey>& _ciphertexts,
        const std::vector<std::vector<uint8_t>>* _aadTE = nullptr) const;

    void useMockCrypto() { doRealCrypto = false; }
};