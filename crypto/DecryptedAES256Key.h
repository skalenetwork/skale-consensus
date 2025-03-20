#pragma once

class DecryptedAES256Key {
protected:
    block_id blockId = 0;

    uint64_t totalNodes = 0;
    uint64_t requiredNodes = 0;

public:
    DecryptedAES256Key( const block_id& blockId, uint64_t _totalNodes, uint64_t _requiredNodes );

    [[nodiscard]] block_id getBlockId() const;

    virtual string toString() = 0;

    virtual ~DecryptedAES256Key();
};

