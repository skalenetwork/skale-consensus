//
// Created by stan on 15-03-2025.
//

#ifndef SKALED_BLOCKHEADEROBJECT_H
#define SKALED_BLOCKHEADEROBJECT_H

#include "FlatBufferRequest.h"

namespace block_finalize {

class BlockHeaderObject {
private:
    schain_id schainId;
    epoch_id epochId;
    block_id blockId;
    schain_index proposerIndex;
    uint64_t transactionCount;
    uint64_t timeStampS;
    uint64_t timeStampMs;
    ptr<sha_hash> transactionsMerkleRoot;
    ptr<sha_hash> parentHash;
    ptr<extra_data> extraData;

public:
    // Constructor
    BlockHeaderObject(schain_id schainId, epoch_id epochId, block_id blockId, schain_index proposerIndex,
                      uint64_t transactionCount, uint64_t timeStampS, uint64_t timeStampMs,
                      ptr<sha_hash>& transactionsMerkleRoot, ptr<sha_hash>& parentHash,
                      ptr<extra_data>& extraData);
    // Getters
    [[nodiscard]] schain_id getSchainId() const { return schainId; }
    [[nodiscard]] epoch_id getEpochId() const { return epochId; }
    [[nodiscard]] block_id getBlockId() const { return blockId; }
    [[nodiscard]] schain_index getProposerIndex() const { return proposerIndex; }
    [[nodiscard]] uint64_t getTransactionCount() const { return transactionCount; }
    [[nodiscard]] uint64_t getTimeStampS() const { return timeStampS; }
    [[nodiscard]] uint64_t getTimeStampMs() const { return timeStampMs; }
    [[nodiscard]] const ptr<sha_hash>& getTransactionsMerkleRoot() const { return transactionsMerkleRoot; }
    [[nodiscard]] const ptr<sha_hash>& getParentHash() const { return parentHash; }
    [[nodiscard]] const ptr<extra_data>& getExtraData() const { return extraData; }

    // Deserialize function
    static std::unique_ptr<BlockHeaderObject> deserializeAndVerify(block_finalize::BlockHeader * _fbBlockHeader, schain_id _schainId) noexcept(false);
};

}  // namespace block_finalize

#endif  // SKALED_BLOCKHEADEROBJECT_H
