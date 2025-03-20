#pragma once

class DecryptedAESKey {
protected:
    block_id blockId = 0;

    uint64_t totalDecryptors = 0;
    uint64_t requiredDecryptors = 0;

public:
    DecryptedAESKey( const block_id& blockId, uint64_t _totalDecryptors, uint64_t _requiredDecryptors );

    [[nodiscard]] block_id getBlockId() const;

    virtual string toString() = 0;

    virtual ~DecryptedAESKey();
};

