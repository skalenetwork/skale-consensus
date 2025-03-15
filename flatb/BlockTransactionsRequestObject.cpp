//
// Created by stan on 15-03-2025.
//



#include <iostream>
#include <array>
#include <folly/io/IOBuf.h>
#include "block_transactions_request_generated.h"  // Include FlatBuffers-generated headers
#include "SkaleCommon.h"
#include "BlockTransactionsRequestObject.h"

using namespace block_finalize;

unique_ptr< BlockTransactionsRequestObject > BlockTransactionsRequestObject::deserializeAndVerify(
    const folly::IOBuf& _buffer, schain_id _sChainId ) {
    const block_finalize::BlockTransactionsRequest* request = nullptr;
    VERIFY_AND_GET_REQUEST( _buffer, BlockTransactionsRequest, request );

    auto transactionIndices = request->transaction_indices();

    if (!transactionIndices) {
        throw std::invalid_argument("Null transaction indices in BlockTransactionsRequest request");
    }

    auto result =  std::make_unique< BlockTransactionsRequestObject >( request->schain_id(),
        request->epoch_id(), request->block_id(), request->node_id(), request->proposer_index(),
        *transactionIndices);
    result->verify(_sChainId);
    return result;
}