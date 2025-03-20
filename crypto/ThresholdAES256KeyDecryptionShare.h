#pragma once

class BLAKE3Hash;


class ThresholdAES256KeyDecryptionShare {
protected:
    schain_id schainId = 0;
    block_id blockId = 0;
    transaction_index transactionIndex = 0;
    schain_index decryptorIndex = 0;


public:
    [[nodiscard]] block_id getBlockId() const;

    [[nodiscard]] transaction_index getTransactionIndex() const;

    ThresholdAES256KeyDecryptionShare(
        const schain_id& _schainId, const block_id& _blockId,
        const transaction_index& _transactionIndex,
        const schain_index& _decryptorIndex );

    virtual string toString() = 0;

    virtual ~ThresholdAES256KeyDecryptionShare();

    [[nodiscard]] schain_index getDecryptorIndex() const;
    BLAKE3Hash computeHash();
};

