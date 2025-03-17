//
// Created by stan on 15-03-2025.
//

#ifndef SKALED_BLOCKHEADEROBJECT_H
#define SKALED_BLOCKHEADEROBJECT_H

#include "FlatBufferRequest.h"
#include "Hash.h"  // Assuming Hash is defined elsewhere

namespace block_finalize {

class BlockHeaderObject : public FlatBufferRequest {
private:
    schain_id schainId;
    epoch_id epochId;
    block_id blockId;
    schain_index proposerIndex;
    uint64_t transactionCount;
    uint64_t timeStampS;
    uint64_t timeStampMs;
    Hash transactionsMerkleRoot;
    Hash parentHash;
    std::array<uint8_t, 32> extraData;

public:
    // Constructor
    BlockHeaderObject(schain_id schainId, epoch_id epochId, block_id blockId, schain_index proposerIndex,
                      uint64_t transactionCount, uint64_t timeStampS, uint64_t timeStampMs,
                      const Hash& transactionsMerkleRoot, const Hash& parentHash,
                      const std::array<uint8_t, 32>& extraData)
        : FlatBufferRequest(schainId, epochId, blockId, proposerIndex),
          schainId(schainId), epochId(epochId), blockId(blockId), proposerIndex(proposerIndex),
          transactionCount(transactionCount), timeStampS(timeStampS), timeStampMs(timeStampMs),
          transactionsMerkleRoot(transactionsMerkleRoot), parentHash(parentHash), extraData(extraData) {}

    // Getters
    [[nodiscard]] schain_id getSchainId() const { return schainId; }
    [[nodiscard]] epoch_id getEpochId() const { return epochId; }
    [[nodiscard]] block_id getBlockId() const { return blockId; }
    [[nodiscard]] schain_index getProposerIndex() const { return proposerIndex; }
    [[nodiscard]] uint64_t getTransactionCount() const { return transactionCount; }
    [[nodiscard]] uint64_t getTimeStampS() const { return timeStampS; }
    [[nodiscard]] uint64_t getTimeStampMs() const { return timeStampMs; }
    [[nodiscard]] const Hash& getTransactionsMerkleRoot() const { return transactionsMerkleRoot; }
    [[nodiscard]] const Hash& getParentHash() const { return parentHash; }
    [[nodiscard]] const std::array<uint8_t, 32>& getExtraData() const { return extraData; }

    // Deserialize function
    static std::unique_ptr<BlockHeaderObject> deserializeAndVerify(const folly::IOBuf& _buffer, schain_id _schainId) noexcept(false);
};

}  // namespace block_finalize

#endif  // SKALED_BLOCKHEADEROBJECT_H
