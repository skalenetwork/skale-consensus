//
// Created by stan on 15-03-2025.
//

#ifndef SKALED_BLOCKFINALIZEREQUESTOBJECT_H
#define SKALED_BLOCKFINALIZEREQUESTOBJECT_H


#include "FlatBufferRequest.h"


namespace folly {
class IOBuf;
}

namespace block_finalize {

class BlockFinalizeRequestObject : public FlatBufferRequest {
private:

    bool needDaProofSig;
    bool needDecryptionShares;
    bool needFragment;
    fragment_index fragmentIndex;

public:

    BlockFinalizeRequestObject(schain_id schainId, epoch_id epochId, block_id blockId, node_id nodeId,
        schain_index proposerIndex, bool needDaProofSig, bool needDecryptionShares,
        bool needFragment, fragment_index fragmentIndex)
        : FlatBufferRequest(schainId, epochId, blockId, nodeId, proposerIndex), // Initialize base class
          needDaProofSig(needDaProofSig),
          needDecryptionShares(needDecryptionShares),
          needFragment(needFragment),
          fragmentIndex(fragmentIndex) {}

    std::unique_ptr<BlockFinalizeRequestObject> deserialize( const folly::IOBuf& _buffer ) noexcept(false);
};
}


#endif  // SKALED_BLOCKFINALIZEREQUESTOBJECT_H
