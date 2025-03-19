#pragma once


#include "FlatBufferRequest.h"

class Schain;

namespace skale_fb {

class BlockTransactionsRequestObject : public FlatBufferRequest {
private:
    ptr<vector< transaction_index >> transactionIndices;

public:
    // Constructor
    BlockTransactionsRequestObject( schain_id schainId, epoch_id epochId, block_id blockId,
        node_id nodeId, schain_index proposerIndex,
        ptr<vector< transaction_index >>& transactionIndices )
        : FlatBufferRequest( schainId, epochId, blockId, nodeId, proposerIndex ),
          transactionIndices( transactionIndices) {
            CHECK_STATE(transactionIndices);
        }

    // Getter for transactions
    [[nodiscard]] const ptr<vector< transaction_index >>& getTransactionIndices() const { return transactionIndices; }

    // Deserialize function
    static ptr< BlockTransactionsRequestObject > deserializeAndVerify(const folly::IOBuf& _buffer,
        schain_id _schainId) noexcept( false );

};

}  // namespace block_finalize
