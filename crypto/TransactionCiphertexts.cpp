#include "TransactionCiphertexts.h"
#include "libBLS/threshold_encryption/threshold_encryption.h"

TransactionCiphertexts::TransactionCiphertexts(ptr<BiteCiphertext> ciphertext) {
    ciphertexts.push_back(ciphertext->getEncryptedAESKey());
}

TransactionCiphertexts::TransactionCiphertexts(std::vector<ptr<BiteCiphertext>>& ciphertextsVec) {
    for (const auto& ciphertext : ciphertextsVec) {
        ciphertexts.push_back(ciphertext->getEncryptedAESKey());
    }
}