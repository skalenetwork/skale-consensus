#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#include <boost/container/flat_map.hpp>
#include "datastructures/SmallVector.h"
#include "crypto/AESKeyDecryptionShare.h"
#pragma GCC diagnostic pop
class AESKeyDecryptionShare;

/**
 * @brief Holds a list of AES key decryption shares from a specific decryptor node for all transactions in a block.
 * For each transaction, there may be multiple ciphertexts, and thus multiple decryption shares.
 * Decryption shares should all follow the same order - the order of ciphertexts in the original transaction.
 * That is, share 1 for tx 1 corresponds to the same ciphertext as share 1 for tx 1 from another decryptor.
 */
class AESKeyDecryptionShareList {
    block_id blockId;
    schain_index proposerIndex;
    schain_index decryptorIndex;

private:
    // Maps a tx index to a list of shares - one per ciphertext in the current tx
    boost::container::flat_map<transaction_index, ptr<AESKeyDecryptionShares>> decryptionShares;
    size_t totalCiphertextShares = 0;

public:
    AESKeyDecryptionShareList(const block_id &_blockId, schain_index _proposerIndex, const schain_index & _decryptorIndex )
        : blockId(_blockId),
          proposerIndex(_proposerIndex), decryptorIndex( _decryptorIndex ) {
    }

    [[nodiscard]] schain_index getProposerIndex() const {
        return proposerIndex;
    }


    block_id getBlockId() const;

    schain_index getDecryptorIndex() const;

    // returns number of transactions with decryption shares
    size_t size() const {
        return decryptionShares.size();
    }

    // returns total count of ciphertext shares across all transactions
    size_t totalCiphertextSharesCount() const {
        return totalCiphertextShares;
    }

    [[nodiscard]] const boost::container::flat_map<transaction_index, ptr<AESKeyDecryptionShares>>& getDecryptionShares() const {
        return decryptionShares;
    }

    void reserve(size_t _size);

    /**
     * @brief Adds decryption shares for a given transaction index.
     * Note that decryptShares may contain multiple shares, or a single share.
     */
    void addShares(transaction_index _index, const ptr<AESKeyDecryptionShares> &_decryptShares);

    ptr<AESKeyDecryptionShares> getDecryptionShares(transaction_index _transactionIndex) const;

};

