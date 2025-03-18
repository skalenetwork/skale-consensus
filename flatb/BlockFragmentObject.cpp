//
// Created by stan on 18-03-2025.
//

#include "block_finalize_request_generated.h"
#include "SkaleCommon.h"
#include "FlatBufferRequest.h"
#include "Log.h"
#include "BlockFragmentObject.h"

using namespace block_finalize;

// Constructor


BlockFragmentObject::BlockFragmentObject(uint32_t index, ptr<vector<ptr<partial_sha_hash>>> &_txTruncatedHashes,
    ptr<vector<ptr<sha_hash>>> &_leftProof, ptr<vector<ptr<sha_hash>>> &_rightProof)
    : index(index),
      txTruncatedHashes(_txTruncatedHashes),
      leftProof(_leftProof),
      rightProof(_rightProof) {
    CHECK_STATE(index < Node::getNodeCount());
    CHECK_STATE(index > 0)
    CHECK_STATE(index <= Node::getNodeCount() - 1 );
}


ptr<BlockFragmentObject> BlockFragmentObject::deserializeAndVerify(const BlockFragment* _fbBlockFragment) {

    if (!_fbBlockFragment) {
        return nullptr;
    }


    auto leftProof = make_shared<vector<ptr<sha_hash>>>();
    auto rightProof = make_shared<vector<ptr<sha_hash>>>();
    auto txTruncatedHashes = make_shared<vector<ptr<partial_sha_hash>>>();

    // Copy data using lambda
    FlatBufferRequest::copyFbHashList(txTruncatedHashes, _fbBlockFragment->tx_truncated_hashes(), PARTIAL_HASH_LEN);
    FlatBufferRequest::copyFbHashList(leftProof, _fbBlockFragment->left_proof(), HASH_LEN);
    FlatBufferRequest::copyFbHashList(rightProof, _fbBlockFragment->right_proof(), HASH_LEN);


    auto result = std::make_shared<BlockFragmentObject>(_fbBlockFragment->index(), txTruncatedHashes, leftProof, rightProof);
    return result;
}
