#pragma once

#include "AESKeyDecryptionShareSet.h"
#include "bite/crypto/DecryptedAESKey.h"
#include "crypto/EncryptedAESKey.h"

class PartialHashesList;
class Schain;
class BLAKE3Hash;
#include "crypto/AESKeyDecryptionShare.h"

/**
 * @brief Holds a set of AES key decryption shares from consensus nodes for a specific transaction.
 * Each transaction may have multiple ciphertexts (EncryptedAESKeys), and for each ciphertext,
 * there is a corresponding TEDecryptSet to hold the decryption shares.
 * 
 * This class treats each transaction (not each ciphertext) as a unit.
 */
class ConsensusAESKeyDecryptionShareSet : public AESKeyDecryptionShareSet {
    // N ciphertexts
    ptr<EncryptedAESKeys> encryptedAESKeys;
    // 1 decrypt set for each ciphertext within this transaction
    small_vector<libBLS::TEDecryptSet> decryptionSets;  // tsafe
    recursive_mutex decryptionSharesLock;

public:
    ConsensusAESKeyDecryptionShareSet( block_id _blockId, transaction_index _transactionIndex, size_t numCiphertexts,
        size_t _totalDecryptors, size_t _requiredDecryptors);


    /**
     * @brief Verifies and merges the AES keys from all collected shares for all ciphertexts.
     * @param _encryptedAESKey The encrypted AES keys corresponding to the ciphertexts.
     * It uses the collected shares for each ciphertext, and merges each set of shares 
     * individually to reconstruct the original AES keys, one at a time.
     * @return Pointer to 1 decrypted AES key for each ciphertext.
     * If input contains 3 args, then the set will contain N * 3 total shares, and will output 3 decrypted AES keys.
     * Index 0 of the returned value corresponds to ciphertext 0, index 1 to ciphertext 1, and so on.
     * Same for input.
     */
    ptr<DecryptedAESKeys> verifyAndMergeAESKeys(EncryptedAESKeys& _encryptedAESKey) override;

    virtual bool addDecryptionSharesFromSameDecryptor(
        const ptr< AESKeyDecryptionShares >& _decryptionShares ) override;

    bool isEnough() override;

    ~ConsensusAESKeyDecryptionShareSet() override;
};
