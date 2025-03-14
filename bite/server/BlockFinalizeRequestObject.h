//
// Created by stan on 14-03-2025.
//

#ifndef SKALED_BLOCKFINALIZEREQUESTOBJECT_H
#define SKALED_BLOCKFINALIZEREQUESTOBJECT_H


#ifndef BLOCK_FINALIZE_REQUEST_OBJECT_HPP
#define BLOCK_FINALIZE_REQUEST_OBJECT_HPP

#include <stdexcept>
#include <iostream>
#include <folly/io/IOBuf.h>
#include "flatb/block_finalize_request_generated.h" // Include the generated FlatBuffer schema

class BlockFinalizeRequestObject {
public:
    BlockFinalizeRequestObject(uint64_t schainId, uint64_t epochId, uint64_t blockId,
        uint64_t nodeId, uint64_t proposerIndex, bool needDaProofSig,
        bool needDecryptionShares, bool needFragment, uint64_t fragmentIndex)
        : schainId(schainId),
          epochId(epochId),
          blockId(blockId),
          nodeId(nodeId),
          proposerIndex(proposerIndex),
          needDaProofSig(needDaProofSig),
          needDecryptionShares(needDecryptionShares),
          needFragment(needFragment),
          fragmentIndex(fragmentIndex) {}

    // Getter methods for accessing private members
    uint64_t getSchainId() const { return schainId; }
    uint64_t getEpochId() const { return epochId; }
    uint64_t getBlockId() const { return blockId; }
    uint64_t getNodeId() const { return nodeId; }
    uint64_t getProposerIndex() const { return proposerIndex; }
    bool getNeedDaProofSig() const { return needDaProofSig; }
    bool getNeedDecryptionShares() const { return needDecryptionShares; }
    bool getNeedFragment() const { return needFragment; }
    uint64_t getFragmentIndex() const { return fragmentIndex; }

    // Static method to deserialize from a FlatBuffer
    static BlockFinalizeRequestObject deserialize(const folly::IOBuf& buffer) {


        // Get buffer data and size (assuming already coalesced)
        const uint8_t* buffer_data = buffer.data();
        size_t buffer_size = buffer.length();

        // Verify the buffer using FlatBuffers Verifier
        flatbuffers::Verifier verifier(buffer_data, buffer_size);
        if (!block_finalize::VerifyBlockFinalizeRequestBuffer(verifier)) {
            throw std::invalid_argument("Error: Invalid FlatBuffer data");
        }

        // Deserialize the object
        auto request = flatbuffers::GetRoot<block_finalize::BlockFinalizeRequest>(buffer_data);

        return { request->schain_id(), request->epoch_id(), request->block_id(), request->node_id(),
            request->proposer_index(), request->need_da_proof_sig(), request->need_decryption_shares(),
            request->need_fragment(), request->fragment_index() };
    }

private:
    uint64_t schainId;
    uint64_t epochId;
    uint64_t blockId;
    uint64_t nodeId;
    uint64_t proposerIndex;
    bool needDaProofSig;
    bool needDecryptionShares;
    bool needFragment;
    uint64_t fragmentIndex;
};

#endif // BLOCK_FINALIZE_REQUEST_OBJECT_HPP

#endif  // SKALED_BLOCKFINALIZEREQUESTOBJECT_H
