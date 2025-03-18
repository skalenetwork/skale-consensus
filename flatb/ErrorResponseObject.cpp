//
// Created by kladko on 3/18/25.
//

#include "block_finalize_request_generated.h"
#include "SkaleCommon.h"
#include "Log.h"
#include "FlatBufferRequest.h"
#include "ErrorResponseObject.h"

using namespace block_finalize;

// Constructor
ErrorResponseObject::ErrorResponseObject(uint32_t _status, uint32_t _substatus, uint64_t _lastBlock,
                                         uint64_t _lastBlockTimestampS, uint64_t _lastBlockTimestampMs,  const std::string &_message)
    : status(_status), substatus(_substatus), lastBlock(_lastBlock),
      lastBlockTimestampS(_lastBlockTimestampS), lastBlockTimestampMs(_lastBlockTimestampMs), message(_message) {
    CHECK_STATE2(!message.empty(), "ErrorResponse message cannot be empty");
}

// Deserialize and verify function
std::shared_ptr<ErrorResponseObject> ErrorResponseObject::deserializeAndVerify(const ErrorResponse* _fbErrorResponse) {
    if (!_fbErrorResponse) {
        return nullptr;
    }

    // Extract values from FlatBuffer
    uint32_t status = _fbErrorResponse->status();
    uint32_t substatus = _fbErrorResponse->substatus();
    uint64_t lastBlock = _fbErrorResponse->last_block();
    uint64_t lastBlockTimestampS = _fbErrorResponse->last_block_timestamp_s();
    uint64_t lastBlockTimestampMs = _fbErrorResponse->last_block_timestamp_ms();

    CHECK_STATE(_fbErrorResponse->message());

    std::string message = _fbErrorResponse->message()->str();
    CHECK_STATE2(!message.empty(), "ErrorResponse message cannot be empty");

    return std::make_shared<ErrorResponseObject>(status, substatus, lastBlock, lastBlockTimestampS,
        lastBlockTimestampMs, message);
}
