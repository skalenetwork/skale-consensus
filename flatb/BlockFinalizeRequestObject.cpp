//
// Created by stan on 15-03-2025.
//

#include <folly/io/IOBuf.h>
#include "block_finalize_request_generated.h"
#include "SkaleCommon.h"
#include "Log.h"
#include "node/Node.h"
#include "FlatBufferRequest.h"
#include "BlockFinalizeRequestObject.h"

using namespace skale_fb;

ptr<BlockFinalizeRequestObject> BlockFinalizeRequestObject::deserializeAndVerify(
    const folly::IOBuf& _buffer) {
    const BlockFinalizeRequest* request = nullptr;
    VERIFY_AND_PARSE_FLATBUFFER( _buffer, BlockFinalizeRequest, request );

    auto result =  make_shared< BlockFinalizeRequestObject >( request->schain_id(),
        request->epoch_id(), request->block_id(), request->node_id(), request->proposer_index(),
        request->need_da_proof_sig(), request->need_decryption_shares(), request->need_fragment(),
        request->fragment_index() );
    return result;
}