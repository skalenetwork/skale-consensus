//
// Created by kladko on 3/17/25.
//


//
// Created by stan on 15-03-2025.
//

#include <folly/io/IOBuf.h>
#include "block_finalize_request_generated.h"
#include "SkaleCommon.h"
#include "FlatBufferRequest.h"
#include "Log.h"
#include "BlockHeaderObject.h"

using namespace block_finalize;


// Constructor
BlockHeaderObject::BlockHeaderObject(schain_id schainId, epoch_id epochId, block_id blockId, schain_index proposerIndex,
                  uint64_t transactionCount, uint64_t timeStampS, uint64_t timeStampMs,
                  ptr<sha_hash>& transactionsMerkleRoot, ptr<sha_hash>& parentHash,
                  ptr<extra_data>& extraData)
    : schainId(schainId), epochId(epochId), blockId(blockId), proposerIndex(proposerIndex),
      transactionCount(transactionCount), timeStampS(timeStampS), timeStampMs(timeStampMs),
      transactionsMerkleRoot(transactionsMerkleRoot), parentHash(parentHash), extraData(extraData) {
    CHECK_STATE(schainId == Node::getSchainId());
    CHECK_STATE(proposerIndex <= (uint64_t) Node::getNodeCount());
}

std::unique_ptr<BlockHeaderObject> BlockHeaderObject::deserializeAndVerify(block_finalize::BlockHeader * _fbBlockHeader,
    schain_id _schainId) {

    if (_fbBlockHeader == nullptr) {
        return nullptr;
    }


    auto tmr = make_shared<sha_hash>();
    auto ph = make_shared<sha_hash>(); ;
    auto ed = make_shared<::extra_data>();
    FlatBufferRequest::copyFbdata(_fbBlockHeader->transactions_merkle_root().data(), *tmr);
    FlatBufferRequest::copyFbdata(_fbBlockHeader->parent_hash().data(), *ph);
    FlatBufferRequest::copyFbdata(_fbBlockHeader->extra_data(), *ed);


    auto result = std::make_unique<BlockHeaderObject>(
        _fbBlockHeader->schain_id(), _fbBlockHeader->epoch_id(), _fbBlockHeader->block_id(), _fbBlockHeader->proposer_index(),
        _fbBlockHeader->transaction_count(), _fbBlockHeader->time_stamp_s(), _fbBlockHeader->time_stamp_ms(),
        tmr, ph, ed);
    return result;
}
