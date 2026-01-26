#include "TransactionCiphertexts.h"
#include "libBLS/threshold_encryption/threshold_encryption.h"

TransactionCiphertexts::TransactionCiphertexts(ptr<BiteCiphertext> ciphertext, 
    std::optional<AddressBytes> _scAddressAadTE) 
    : scAddressAadTE(std::move(_scAddressAadTE)) {
    ciphertexts.push_back(ciphertext->getEncryptedAESKey());
}

TransactionCiphertexts::TransactionCiphertexts(std::vector<ptr<BiteCiphertext>>& ciphertextsVec,
    std::optional<AddressBytes> _scAddressAadTE)
    : scAddressAadTE(std::move(_scAddressAadTE)) {
    for (const auto& ciphertext : ciphertextsVec) {
        ciphertexts.push_back(ciphertext->getEncryptedAESKey());
    }
}