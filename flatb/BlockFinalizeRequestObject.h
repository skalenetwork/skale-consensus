#pragma once

#include "FlatBufferRequest.h"


namespace folly {
class IOBuf;
}

namespace block_finalize {

class BlockFinalizeRequestObject : public FlatBufferRequest {
public:
    [[nodiscard]] bool getNeedDaProofSig() const {
        return needDaProofSig;
    }

    [[nodiscard]] bool getNeedDecryptionShares() const {
        return needDecryptionShares;
    }

    [[nodiscard]] bool getNeedFragment() const {
        return needFragment;
    }

    [[nodiscard]] fragment_index getNeedFragmentIndex() const {
        return fragmentIndex;
    }

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

    ptr<BlockFinalizeRequestObject> deserializeAndVerify( const folly::IOBuf& _buffer) noexcept(false);
};
}
