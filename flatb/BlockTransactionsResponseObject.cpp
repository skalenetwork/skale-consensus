//
// Created by kladko on 3/18/25.
//

#include <iostream>
#include <folly/io/IOBuf.h>
#include "block_transactions_response_generated.h"

#undef LOG // address incompatibility with folly
#include "Log.h"
#include "BlockTransactionsRequestObject.h"
#include "BlockTransactionsResponseObject.h"
#include "FlatBufferRequest.h"


using namespace block_finalize;

BlockTransactionsResponseObject::BlockTransactionsResponseObject(
    std::shared_ptr<std::vector<std::vector<uint8_t>>> &_transactions) : transactions(_transactions) {
    CHECK_STATE2(transactions, "Null transactions in response");
}

std::unique_ptr<BlockTransactionsResponseObject> BlockTransactionsResponseObject::deserializeAndVerify(
    const folly::IOBuf &_buffer, std::shared_ptr<BlockTransactionsRequestObject> &_request) {
    CHECK_STATE(_request);

    const block_finalize::BlockTransactionsResponse *response = nullptr;
    VERIFY_AND_PARSE_FLATBUFFER(_buffer, BlockTransactionsResponse, response);

    if (response->result_type() == block_finalize::BlockTransactionsResult_BlockTransactionsSuccessResponse) {
        const auto *successResponse = response->result_as_BlockTransactionsSuccessResponse();
        CHECK_STATE(successResponse);

        auto transactions = std::make_shared<std::vector<std::vector<uint8_t>>>();


        CHECK_STATE2(successResponse->transactions()->size() == _request->getTransactionIndices()->size(),
            "Response transaction count not equal to request transaction count");
        transactions->reserve(successResponse->transactions()->size());

        for (const auto *fbTransaction : *successResponse->transactions()) {
            CHECK_STATE(fbTransaction);
            auto transactionData = FlatBufferRequest::copyFbByteVector(fbTransaction->data());
            CHECK_STATE(transactionData);
            transactions->push_back(*transactionData);
        }

        return std::make_unique<BlockTransactionsResponseObject>(transactions);
    } else {
        // TODO: Handle ErrorResponse case
        /* const auto *errorResponse = response->result_as_ErrorResponse();
           return std::make_unique<ErrorResponseObject>(
               errorResponse->status(), errorResponse->substatus(), errorResponse->last_block(),
               errorResponse->last_block_timestamp(), errorResponse->message()->str());
        */
    }
}
