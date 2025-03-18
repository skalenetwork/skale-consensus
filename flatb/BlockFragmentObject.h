#pragma once

#include "FlatBufferRequest.h"

namespace block_finalize {

    class BlockFragmentObject {
    private:
        uint32_t index;
        ptr<vector<ptr<partial_sha_hash>>>  txTruncatedHashes;
        ptr<vector<ptr<sha_hash>>>  leftProof;
        ptr<vector<ptr<sha_hash>>>& rightProof;

    public:
        // Constructor
        BlockFragmentObject(uint32_t index, ptr<vector<ptr<partial_sha_hash>>>& _txTruncatedHashes,
                             ptr<vector<ptr<sha_hash>>>& _leftProof, ptr<vector<ptr<sha_hash>>>& _rightProof);

    private:
        BlockFragmentObject(uint32_t index, const ptr<vector<partial_sha_hash>> &tx_truncated_hashes,
            const ptr<vector<sha_hash>> &left_proof, ptr<vector<sha_hash>> &right_proof);
    public:

        [[nodiscard]] uint64_t getIndex() const { return index; }
        [[nodiscard]] const ptr<vector<ptr<partial_sha_hash>>>& getTxTruncatedHashes() const { return txTruncatedHashes; }
        [[nodiscard]] const ptr<vector<ptr<sha_hash>>>& getLeftProof() const { return leftProof; }
        [[nodiscard]] const ptr<vector<ptr<sha_hash>>>&  getRightProof() const { return rightProof; }


        static ptr<BlockFragmentObject> deserializeAndVerify(const BlockFragment* _fbBlockFragment) noexcept(false);
    };

}  // namespace block_finalize
