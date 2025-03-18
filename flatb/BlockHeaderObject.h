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
    ptr<sha_hash> commiteeHash;
    ptr<sha_hash> publicKeyHash;
    ptr<vector<transaction_index>> encryptedTransactionIndices;



public:
    BlockHeaderObject(schain_id _schainId, epoch_id _epochId, block_id _blockId, schain_index _proposerIndex,
                      uint64_t _transactionCount, uint64_t _timeStampS, uint64_t _timeStampMs,
                      ptr<sha_hash>& _transactionsMerkleRoot, ptr<sha_hash>& _parentHash,
                      ptr<extra_data>& _extraData, ptr<sha_hash>& _commiteeHash, ptr<sha_hash>& _publicKeyHash,
                      ptr<vector<transaction_index>>& _encryptedTransactionIndices);
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
    [[nodiscard]] const ptr<vector<transaction_index>>& getEncryptedTransactionIndices() const{ return encryptedTransactionIndices; }
    [[nodiscard]] const ptr<sha_hash>& getCommitteeHash() const{ return commiteeHash; }
    [[nodiscard]] const ptr<sha_hash>& getPublicKeyHash() const{ return publicKeyHash; }

    static ptr<BlockHeaderObject> deserializeAndVerify(const BlockHeader * _fbBlockHeader) noexcept(false);
};

}  // namespace block_finalize

#endif  // SKALED_BLOCKHEADEROBJECT_H
