#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#include <boost/container/flat_map.hpp>
#pragma GCC diagnostic pop

class AESKeyDecryptionShare;

class AESKeyDecryptionShareList {
    block_id blockId;
    schain_index encryptorIndex;

    boost::container::flat_map<transaction_index, ptr<AESKeyDecryptionShare> > decryptionShares;
    std::atomic<bool> isComplete = false;

public:
    AESKeyDecryptionShareList(const block_id &block_id, const schain_index &encryptor_index)
        : blockId(block_id),
          encryptorIndex(encryptor_index) {
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
    void addKey(transaction_index _index, const ptr<AESKeyDecryptionShare> &_decryptShare);

    ptr<AESKeyDecryptionShare> getDecryptionShare(uint64_t id) const;

    void markComplete();

    string serializeToString();
};
