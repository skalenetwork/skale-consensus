#pragma once


#include <boost/container/flat_map.hpp>

class BLAKE3Hash;


class AESKeyDecryptionShare {
protected:
    block_id blockId = 0;
    transaction_index transactionIndex = 0;
    schain_index decryptorIndex = 0;
    bool decryptionFailed = true;


public:


    AESKeyDecryptionShare( const block_id& _blockId,
                           const transaction_index& _transactionIndex,
                           const schain_index& _decryptorIndex, const bool _decryptionFailed );

    virtual string toString() = 0;


    [[nodiscard]] block_id getBlockId() const {
        return blockId;
    }

    [[nodiscard]] transaction_index getTransactionIndex() const {
        return transactionIndex;
    }

    [[nodiscard]] schain_index getDecryptorIndex() const {
        return decryptorIndex;
    }

    [[nodiscard]] bool isDecryptionFailed() const {
        return decryptionFailed;
    }

    virtual ~AESKeyDecryptionShare();

    BLAKE3Hash computeHash();
};
