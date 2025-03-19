#pragma once

#include "FlatBufferRequest.h"
#include "block_finalize_response_generated.h"
#include <vector>
#include <memory>

namespace folly {
    class IOBuf;
}

namespace block_finalize {
    class BlockTransactionsRequestObject;

    class BlockTransactionsResponseObject {
    private:
        std::shared_ptr<std::vector<std::vector<uint8_t>>> transactions;

    public:
        BlockTransactionsResponseObject(std::shared_ptr<std::vector<std::vector<uint8_t>>> &_transactions) noexcept(false);

        static ptr<BlockTransactionsResponseObject> deserializeAndVerify(const folly::IOBuf &_buffer,
            std::shared_ptr<BlockTransactionsRequestObject> &_request);
    };
}
