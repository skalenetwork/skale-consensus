#pragma once

#include "FlatBufferRequest.h"
#include <string>

namespace block_finalize {

    class ErrorResponseObject {
    private:
        uint32_t status;
        uint32_t substatus;
        uint64_t lastBlock;
        uint64_t lastBlockTimestampS;
        uint64_t lastBlockTimestampMs;
        std::string message;

    public:
        // Constructor
        ErrorResponseObject(uint32_t _status, uint32_t _substatus, uint64_t _lastBlock,
                            uint64_t _lastBlockTimestampS, uint64_t _lastBlockTimestampMs, const std::string &_message);

        [[nodiscard]] uint32_t getStatus() const { return status; }
        [[nodiscard]] uint32_t getSubstatus() const { return substatus; }
        [[nodiscard]] uint64_t getLastBlock() const { return lastBlock; }
        [[nodiscard]] uint64_t getLastBlockTimestampS() const { return lastBlockTimestampS; }
        [[nodiscard]] uint64_t getLastBlockTimestampMs() const { return lastBlockTimestampMs; }
        [[nodiscard]] const std::string &getMessage() const { return message; }

        static std::shared_ptr<ErrorResponseObject> deserializeAndVerify(const ErrorResponse* _fbErrorResponse) noexcept(false);
    };

}  // namespace block_finalize
