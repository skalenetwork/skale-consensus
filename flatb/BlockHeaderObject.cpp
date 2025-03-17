//
// Created by kladko on 3/17/25.
//


//
// Created by stan on 15-03-2025.
//

#include "BlockHeaderObject.h"

using namespace block_finalize;

std::unique_ptr<BlockHeaderObject> BlockHeaderObject::deserializeAndVerify(
    const folly::IOBuf& _buffer, schain_id _schainId) {

    const block_finalize::BlockHeader* header = nullptr;
    VERIFY_AND_PARSE_FLATBUFFER(_buffer, BlockHeader, header);

    if (!header) {
        throw std::invalid_argument("Null BlockHeader in request");
    }

    Hash transactionsMerkleRoot;
    Hash parentHash;
    std::array<uint8_t, 32> extraData;

    // Copy hashes
    std::memcpy(&transactionsMerkleRoot, header->transactions_merkle_root(), sizeof(Hash));
    std::memcpy(&parentHash, header->parent_hash(), sizeof(Hash));

    // Copy extra data
    if (header->extra_data()) {
        std::memcpy(extraData.data(), header->extra_data()->data(), 32);
    } else {
        extraData.fill(0);
    }

    auto result = std::make_unique<BlockHeaderObject>(
        header->schain_id(), header->epoch_id(), header->block_id(), header->proposer_index(),
        header->transaction_count(), header->time_stamp_s(), header->time_stamp_ms(),
        transactionsMerkleRoot, parentHash, extraData
    );

    result->verify(_schainId);
    return result;
}
