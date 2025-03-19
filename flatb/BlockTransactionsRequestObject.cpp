//
// Created by stan on 15-03-2025.
//



#include <iostream>
#include <array>
#include <folly/io/IOBuf.h>
#include "block_transactions_request_generated.h"  // Include FlatBuffers-generated headers
#undef LOG // avoid conflict with folly
#include "SkaleCommon.h"
#include "Log.h"
#include "BlockTransactionsRequestObject.h"

using namespace skale_fb;

ptr< BlockTransactionsRequestObject > BlockTransactionsRequestObject::deserializeAndVerify(
    const folly::IOBuf& _buffer, schain_id _sChainId ) {
    const skale_fb::BlockTransactionsRequest* request = nullptr;
    VERIFY_AND_PARSE_FLATBUFFER( _buffer, BlockTransactionsRequest, request );

    auto fbTransactionIndices = request->transaction_indices();

    if (!fbTransactionIndices) {
        throw std::invalid_argument("Null transaction indices in BlockTransactionsRequest request");
    }

    auto transactionIndices = FlatBufferRequest::copyFbIndexVector(fbTransactionIndices);
    CHECK_STATE(transactionIndices);

    auto result =  make_shared<BlockTransactionsRequestObject>( request->schain_id(),
        request->epoch_id(), request->block_id(), request->node_id(), request->proposer_index(),
        transactionIndices);
    return result;
}