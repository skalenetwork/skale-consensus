//
// Created by stan on 15-03-2025.
//

#ifndef SKALED_BLOCKTRANSACTIONSREQUESTOBJECT_H
#define SKALED_BLOCKTRANSACTIONSREQUESTOBJECT_H


#include "FlatBufferRequest.h"

class Schain;

namespace block_finalize {

class BlockTransactionsRequestObject : public FlatBufferRequest {
private:
    std::vector< uint64_t > transactionIndices;

public:
    // Constructor
    BlockTransactionsRequestObject( schain_id schainId, epoch_id epochId, block_id blockId,
        node_id nodeId, schain_index proposerIndex,
        const ::flatbuffers::Vector<uint64_t>& transactionIndices )
        : FlatBufferRequest( schainId, epochId, blockId, nodeId, proposerIndex ),
          transactionIndices( transactionIndices.begin(), transactionIndices.end() ) {}

    // Getter for transactions
    [[nodiscard]] const std::vector< uint64_t >& getTransactionIndices() const { return transactionIndices; }

    // Deserialize function
    static std::unique_ptr< BlockTransactionsRequestObject > deserializeAndVerify(const folly::IOBuf& _buffer,
        schain_id _schainId) noexcept( false );

};

}  // namespace block_finalize


#endif  // SKALED_BLOCKTRANSACTIONSREQUESTOBJECT_H
