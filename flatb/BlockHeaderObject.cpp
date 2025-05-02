//
// Created by kladko on 3/17/25.
//


//
// Created by stan on 15-03-2025.
//


#include "block_finalize_request_generated.h"
#include "SkaleCommon.h"
#include "FlatBufferRequest.h"
#include "Log.h"
#include "BlockHeaderObject.h"

using namespace skale_fb;


// Constructor
BlockHeaderObject::BlockHeaderObject(schain_id _schainId, epoch_id _epochId, block_id _blockId,
                                     schain_index _proposerIndex,
                                     uint64_t _transactionCount, uint64_t _timeStampS, uint64_t _timeStampMs,
                                     ptr<sha_hash> &_transactionsMerkleRoot, ptr<sha_hash> &_parentHash,
                                     ptr<extra_data> &_extraData, ptr<sha_hash> &_committeeHash, ptr<sha_hash> &
                                     _publicKeyHash,
                                     ptr<vector<transaction_index> > &_encryptedTransactionIndices)
    : schainId(_schainId), epochId(_epochId), blockId(_blockId), proposerIndex(_proposerIndex),
      transactionCount(_transactionCount), timeStampS(_timeStampS), timeStampMs(_timeStampMs),
      transactionsMerkleRoot(_transactionsMerkleRoot), parentHash(_parentHash), extraData(_extraData),
      commiteeHash(_committeeHash), publicKeyHash(_publicKeyHash),
      encryptedTransactionIndices(_encryptedTransactionIndices) {
    CHECK_STATE(_schainId == Node::getSchainId());
    CHECK_STATE(_proposerIndex <= (uint64_t) Node::getNodeCount());
    CHECK_STATE(commiteeHash);
    CHECK_STATE(publicKeyHash);
    CHECK_STATE(encryptedTransactionIndices);
    for (auto &&index: *encryptedTransactionIndices) {
        CHECK_STATE(index < transactionCount);
    }


}

ptr<BlockHeaderObject> BlockHeaderObject::deserializeAndVerify(const BlockHeader *_fbBlockHeader) {
    if (_fbBlockHeader == nullptr) {
        return nullptr;
    }


    auto tmr = make_shared<sha_hash>();
    auto ph = make_shared<sha_hash>();;
    auto ed = make_shared<::extra_data>();
    auto cmh = make_shared<::sha_hash>();
    auto pkh = make_shared<::sha_hash>();


    FlatBufferRequest::copyFbArray(_fbBlockHeader->transactions_merkle_root()->data(), *tmr);
    FlatBufferRequest::copyFbArray(_fbBlockHeader->parent_hash()->data(), *ph);
    FlatBufferRequest::copyFbArray(_fbBlockHeader->committee_hash()->data(), *cmh);
    FlatBufferRequest::copyFbArray(_fbBlockHeader->public_key_hash()->data(), *pkh);
    FlatBufferRequest::copyFbArray(_fbBlockHeader->extra_data(), *ed);
    auto indices = FlatBufferRequest::copyFbIndexVector(_fbBlockHeader->encrypted_transaction_indices());

    // check indices are strictly ascending


    CHECK_STATE2(std::is_sorted(indices->begin(), indices->end()), "Encrypted transaction indices not sorted");

    auto includesDuplicates = std::adjacent_find(indices->begin(), indices->end()) != indices->end();

    CHECK_STATE2(!includesDuplicates, "Encrypted transaction indices include duplicates");

    auto result = std::make_shared<BlockHeaderObject>(
        _fbBlockHeader->schain_id(), _fbBlockHeader->epoch_id(), _fbBlockHeader->block_id(),
        _fbBlockHeader->proposer_index(),
        _fbBlockHeader->transaction_count(), _fbBlockHeader->time_stamp_s(), _fbBlockHeader->time_stamp_ms(),
        tmr, ph, ed, cmh, pkh, indices);
    return result;
}
