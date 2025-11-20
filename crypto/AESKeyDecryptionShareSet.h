#pragma once
#include <datastructures/BlockProposal.h>

class EncryptedAESKey;

#include "crypto/AESKeyDecryptionShare.h"
#include "crypto/DecryptedAESKey.h"
#include "crypto/EncryptedAESKey.h"

class AESKeyDecryptionShareSet {
public:
    AESKeyDecryptionShareSet( const block_id _blockId, transaction_index _transactionIndex );

protected:
    block_id blockId = 0;
    transaction_index transactionIndex = 0;

    static atomic< int64_t > totalObjects;

public:
    virtual ~AESKeyDecryptionShareSet();

    static int64_t getTotalObjects() {
        return totalObjects;
    }

    virtual ptr< DecryptedAESKeys > verifyAndMergeAESKeys(EncryptedAESKeys& _encryptedAesKey) = 0;

    virtual bool isEnough() = 0;

    /**
     * @brief Adds a list of decryption shares corresponding to each ciphertext in a single transaction.
     * @param _decryptionShares The decryption shares to be added. MUST be all from the same decryptor, since
     * they refer to a set of shares all computed for a single transaction (multiple ciphertexts).
     * Meaning index 0 will be share for ciphertext 0, index 1 for ciphertext 1, etc.
     * @return true if the shares were added successfully, false otherwise
     */
    virtual bool addDecryptionSharesFromSameDecryptor(
        const ptr< AESKeyDecryptionShares >& _decryptionShares ) = 0;
};