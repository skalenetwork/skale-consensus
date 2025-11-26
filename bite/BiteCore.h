#pragma once
#include <vector>
#include "SkaleCommon.h"
#include <cstdint>

namespace libBLS {
    class TEPublicKey;
    class AES256Key;
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
        const std::vector<uint8_t>& _plainData ) const;
    
    std::vector<uint8_t> decryptData( const libBLS::AES256Key& _aesKey, 
        std::vector<uint8_t>& _cipherData, bool validate = true ) const;

    CiphertextValidationResult validateCiphertexts(const std::vector<libBLS::CipheredKey>& _decryptedAESKeys) const;
};