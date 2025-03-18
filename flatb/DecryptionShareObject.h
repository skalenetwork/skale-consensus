#pragma once

#include "FlatBufferRequest.h"


namespace block_finalize {

    class DecryptionShareObject {
    private:
        uint32_t transactionIndex;
        ptr<vector<uint8_t>> data;

    public:
        // Constructor
        DecryptionShareObject(uint32_t _transactionIndex, ptr<vector<uint8_t>>& _data);

        [[nodiscard]] uint32_t getTransactionIndex() const { return transactionIndex; }
        [[nodiscard]] const ptr<vector<uint8_t>>& getData() const { return data; }

        static ptr<DecryptionShareObject> deserializeAndVerify(const DecryptionShare* _fbDecryptionShare) noexcept(false);
    };

}  // namespace block_finalize
