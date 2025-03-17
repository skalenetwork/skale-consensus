/*
* Created by stan on 15-03-2025.
 */

#include <iostream>
#include <array>
#include <folly/io/IOBuf.h>
#include "block_finalize_response_generated.h"
#include "SkaleCommon.h"
#include "Log.h"
#include "FlatBufferRequest.h"
#include "BlockFinalizeRequestObject.h"
#include "BlockFinalizeResponseObject.h"

using namespace block_finalize;

BlockFinalizeResponseObject::BlockFinalizeResponseObject(std::shared_ptr<BlockHeader> &_blockHeader,
                                                         std::shared_ptr<vector<uint8_t> > &_blockSig,
                                                         std::shared_ptr<vector<uint8_t> > &_daProofSig,
                                                         std::shared_ptr<BlockFragment> &_blockFragment,
                                                         std::shared_ptr<std::vector<DecryptionShare> > &
                                                         _decryptionShares): blockHeader(_blockHeader),
      blockSig(_blockSig),
      daProofSig(_daProofSig),
      blockFragment(_blockFragment),
      decryptionShares(_decryptionShares) {
    CHECK_STATE2(_blockHeader, "Null block header in response");
    CHECK_STATE2(_blockSig, "Null block sig in response");
}

std::unique_ptr<BlockFinalizeResponseObject> BlockFinalizeResponseObject::deserializeAndVerify(
    const folly::IOBuf &_buffer, ptr<BlockFinalizeRequestObject>& _request) {
    CHECK_STATE(_request);

    const block_finalize::BlockFinalizeResponse *response = nullptr;
    VERIFY_AND_PARSE_FLATBUFFER(_buffer, BlockFinalizeResponse, response);

    if (response->result_type() == block_finalize::BlockFinalizeResult_BlockFinalizeSuccessResponse) {
        const auto *successResponse = response->result_as_BlockFinalizeSuccessResponse();
        CHECK_STATE(successResponse);
        auto blockHeaderInPlacepPointer = successResponse->block_header();
        CHECK_STATE(blockHeaderInPlacepPointer);
        auto blockHeader = make_shared<BlockHeader>(*blockHeaderInPlacepPointer);

        auto blockSig = FlatBufferRequest::copyByteVectorFromFlatBuffer(successResponse->block_sig());
        CHECK_STATE2(blockSig, 'No blocksig in response');

        auto daProofSig = FlatBufferRequest::copyByteVectorFromFlatBuffer(successResponse->da_proof_sig());
        CHECK_STATE2(daProofSig || !_request->getNeedDaProofSig(), "No requested da proof sig in response");


        auto daProofSig = FlatBufferRequest::copyByteVectorFromFlatBuffer(successResponse->block_fragment());
        CHECK_STATE2(daProofSig || !_request->getNeedDaProofSig(), "No requested da proof sig in response");




        return std::make_unique<BlockFinalizeResponseObject>(blockHeader,blockSig, daProofSig, blockFragment, decryptionShares);
    } else {
        /*const auto *errorResponse = response->result_as_ErrorResponse();
        return std::make_unique<ErrorResponseObject>(
            errorResponse->status(), errorResponse->substatus(), errorResponse->last_block(),
            errorResponse->last_block_timestamp(), errorResponse->message()->str());
            */
    }
}
