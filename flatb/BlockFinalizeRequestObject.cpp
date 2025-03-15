//
// Created by stan on 15-03-2025.
//

#include <iostream>
#include <array>
#include <folly/io/IOBuf.h>
#include "block_finalize_request_generated.h"
#include "SkaleCommon.h"
#include "FlatBufferRequest.h"
#include "BlockFinalizeRequestObject.h"

using namespace block_finalize;

unique_ptr< BlockFinalizeRequestObject > BlockFinalizeRequestObject::deserialize(
    const folly::IOBuf& _buffer ) {
    const block_finalize::BlockFinalizeRequest* request = nullptr;
    VERIFY_AND_GET_REQUEST( _buffer, BlockFinalizeRequest, request );

    return std::make_unique< BlockFinalizeRequestObject >( request->schain_id(),
        request->epoch_id(), request->block_id(), request->node_id(), request->proposer_index(),
        request->need_da_proof_sig(), request->need_decryption_shares(), request->need_fragment(),
        request->fragment_index() );
}