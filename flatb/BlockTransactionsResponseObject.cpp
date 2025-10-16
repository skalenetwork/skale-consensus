//
// Created by kladko on 3/18/25.
//

#include <iostream>
#include <folly/io/IOBuf.h>
#include "block_transactions_response_generated.h"

#include "Log.h"
#include "BlockTransactionsRequestObject.h"
#include "BlockTransactionsResponseObject.h"
#include "FlatBufferRequest.h"


using namespace skale_fb;

BlockTransactionsResponseObject::BlockTransactionsResponseObject(
    std::shared_ptr<std::vector<std::vector<uint8_t> > > &_transactions) : transactions(_transactions) {
    CHECK_STATE2(transactions, "Null transactions in response");
}

ptr<BlockTransactionsResponseObject> BlockTransactionsResponseObject::deserializeAndVerify(
    const folly::IOBuf &_buffer, std::shared_ptr<BlockTransactionsRequestObject> &_request) {
    CHECK_STATE(_request);

    const BlockTransactionsResponse *response = nullptr;
    VERIFY_AND_PARSE_FLATBUFFER(_buffer, BlockTransactionsResponse, response);

    auto transactions = std::make_shared<std::vector<std::vector<uint8_t> > >();


    CHECK_STATE2(response->transactions()->size() == _request->getTransactionIndices()->size(),
                 "Response transaction count not equal to request transaction count");
    transactions->reserve(response->transactions()->size());

    for (const auto&& fbTransaction: *response->transactions()) {
        CHECK_STATE(fbTransaction);
        auto transactionData = FlatBufferRequest::copyFbByteVector(fbTransaction->data());
        CHECK_STATE(transactionData);
        transactions->push_back(*transactionData);
    }

    return make_shared<BlockTransactionsResponseObject>(transactions);
}
