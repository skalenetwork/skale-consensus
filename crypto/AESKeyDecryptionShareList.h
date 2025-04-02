#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#include <boost/container/flat_map.hpp>
#pragma GCC diagnostic pop

class AESKeyDecryptionShare;

class AESKeyDecryptionShareList {
    block_id blockId;
    schain_index proposerIndex;
    schain_index encryptorIndex;

public:
    [[nodiscard]] boost::container::flat_map<transaction_index, ptr<AESKeyDecryptionShare>> decryption_shares1() const {
        return decryptionShares;
    }

private:
    boost::container::flat_map<transaction_index, ptr<AESKeyDecryptionShare> > decryptionShares;
    std::atomic<bool> isComplete = false;

public:
    AESKeyDecryptionShareList(const block_id &_blockId, schain_index _proposerIndex, const schain_index &_encryptorIndex)
        : blockId(_blockId),
          proposerIndex(_proposerIndex),
          encryptorIndex(_encryptorIndex) {
    }

    [[nodiscard]] schain_index getProposerIndex() const {
        return proposerIndex;
    }


    block_id getBlockId() const;

    schain_index getDecryptorIndex() const;

    size_t getSize() const {
        return decryptionShares.size();
    }

    [[nodiscard]] const boost::container::flat_map<transaction_index, ptr<AESKeyDecryptionShare>>& getDecryptionShares() const {
        return decryptionShares;
    }


    // Optional: Add public access methods
    void addShare(transaction_index _index, const ptr<AESKeyDecryptionShare> &_decryptShare);

    ptr<AESKeyDecryptionShare> getDecryptionShare(transaction_index _transactionIndex) const;

    void markComplete();
};

