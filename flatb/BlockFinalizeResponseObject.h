#pragma once

#include "FlatBufferRequest.h"
#include "block_finalize_response_generated.h"
#include <vector>
#include <optional>
#include <string>

namespace folly {
    class IOBuf;
}


namespace block_finalize {
    class BlockFinalizeRequestObject;
    class BlockHeaderObject;
    class BlockFragmentObject;
    class DecryptionShareObject;

    class BlockFinalizeResponseObject {
    private:
        std::shared_ptr<BlockHeaderObject> blockHeader;
        std::shared_ptr<vector<uint8_t> > blockSig;
        std::shared_ptr<std::vector<uint8_t> > daProofSig;
        std::shared_ptr<BlockFragmentObject> blockFragment;
        std::shared_ptr<std::vector<ptr<DecryptionShareObject>> > decryptionShares;

    public:
        BlockFinalizeResponseObject(std::shared_ptr<BlockHeaderObject> &_blockHeader,
                                    std::shared_ptr<vector<uint8_t> > &_blockSig,
                                    std::shared_ptr<vector<uint8_t> > &_daProofSig,
                                    std::shared_ptr<BlockFragmentObject> &_blockFragment,
                                    std::shared_ptr<std::vector<ptr<DecryptionShareObject>> > &_decryptionShares) noexcept(false);

        static std::unique_ptr<BlockFinalizeResponseObject> deserializeAndVerify(const folly::IOBuf &_buffer,
            std::shared_ptr<BlockFinalizeRequestObject> &_request);
    };
}


