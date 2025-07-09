#include "abstracttcpserver/ConnectionStatus.h"

#include "FinalizeRequestHandler.h"
#undef LOG // avoid macro definition  conflicts with proxygen LOG
#include "Log.h"
#include "flatb/ErrorResponseObject.h"

using namespace std;

constexpr uint64_t MIN_FLATBUFFER_REQUEST_LEN = 16;

void FinalizeRequestHandler::onRequest(std::unique_ptr<HTTPMessage> _headers) noexcept {
    auto requestPath = _headers->getPath();
    if (requestPath == "/block_finalize") {
        requestType = RequestType::BLOCK_FINALIZE;
    } else {
        requestType = RequestType::INVALID;
    }
}

void FinalizeRequestHandler::onBody(std::unique_ptr<folly::IOBuf> _body) noexcept {
    // HTTP2 may send body in chunks, so we are accumulating
    if (_body) {
        bodyQueue.append(move(_body)); // Zero-copy accumulation
    }
}


void FinalizeRequestHandler::onEOM() noexcept {
    string responseMessage;
    string response;
    ConnectionSubStatus errorSubStatus = ConnectionSubStatus::CONNECTION_ERROR_UNKNOWN_SERVER_ERROR;
    string errorMessage = "Unknown error";


    try {
        if (requestType == INVALID) {
            ResponseBuilder(downstream_)
                    .status(404, "Not Found")
                    .body("Unknown endpoint")
                    .sendWithEOM();
            return;
        }

        // Merge chained buffers into a contiguous block (efficient if already contiguous)
        auto request = bodyQueue.move();
        request->coalesce();

        // do sanity check
        if (request->empty()) {
            errorSubStatus = ConnectionSubStatus::CONNECTION_ERROR_CANT_PARSE_REQUEST;
            errorMessage = "Bad Request: Empty FlatBuffer data";
            goto send_error;
        }

        // Check for minimum valid FlatBuffer length (assumed 16 bytes)
        if (request->length() < MIN_FLATBUFFER_REQUEST_LEN) {
            errorSubStatus = ConnectionSubStatus::CONNECTION_ERROR_CANT_PARSE_REQUEST;
            errorMessage =  "FlatBuffer request too short too parse: length: " + to_string(request->length());
            goto send_error;
        }

        // Due to flatbuffer format the fourth byte is always zero
        if (request->data()[4] != 0) {
            errorSubStatus = ConnectionSubStatus::CONNECTION_ERROR_CANT_PARSE_REQUEST;
            errorMessage =  "Bad Request: non-FlatBuffer format";
            goto send_error;;
        }


        try {
            switch (requestType) {
                case RequestType::BLOCK_FINALIZE: {
                    response = getBlockFinalizeResponse(*request);
                    break;
                }
                default:
                    errorSubStatus = CONNECTION_ERROR_UNKNOWN_SERVER_ERROR;
                    errorMessage = string(__CLASS_NAME__) + ":" + to_string(__LINE__) + "Unknown request type";
                    goto send_error;
            }
        } catch (Exception &_e) {
            //TODO
        } catch (...) {
            //TODO
        }

        sendFlatBufferSuccessResponse(response);
    } catch (std::exception &_e) {
        // Something really bad happened so we could not send a response, even an error response
        // tell client to disconnect
        LOG(critical, "Could not send response, disconnecting client " + string( _e.what() ));
        goto send_abort;
    }
    catch (...) {
        // Something really bad happened so we could not send a response, even an error response
        // tell client to disconnect
        LOG(critical, "Could not send response, disconnecting client");
        goto send_abort;
    }

send_error:
    sendFlatBufferError(errorSubStatus, errorMessage);
    return;

send_abort:
    downstream_->sendAbort();

}

void FinalizeRequestHandler::sendHTTPError(uint32_t _errorCode, const string &_message) const {
    ResponseBuilder(downstream_)
            .status(_errorCode, _message)
            .body(_message)
            .sendWithEOM();
}

void FinalizeRequestHandler::sendHTTPResponse(uint16_t _statusCode, const string &_statusMessage,
                                                const string &_body) {
    ResponseBuilder(downstream_)
            .status(_statusCode, _statusMessage)
            .header("Content-Type", "application/octet-stream")
            .body(_body)
            .sendWithEOM();
}


void FinalizeRequestHandler::onError(ProxygenError _err) noexcept {
    LOG(err, "Error in FlatBufferRequestHandler: " + to_string( _err ));
}

void FinalizeRequestHandler::sendFlatBufferSuccessResponse(const string &response) noexcept {
    sendHTTPResponse(200, response, "OK");
}

void FinalizeRequestHandler::sendFlatBufferError(ConnectionSubStatus _substatus, string _message) {
    ErrorResponseObject ero(ConnectionStatus::CONNECTION_ERROR, _substatus, 0, 0, 0,
                            _message);

    auto fbError = ero.serialize();

    ResponseBuilder(downstream_)
            .status(400, _message)
            .body(*fbError)
            .sendWithEOM();
}


string FinalizeRequestHandler::getBlockFinalizeResponse(
    const folly::IOBuf &) noexcept {
    FlatBufferBuilder builder;
    return string(
        reinterpret_cast<const char *>(builder.GetBufferPointer()), builder.GetSize());
}


string FinalizeRequestHandler::getBlockTransactionsResponse(
    const folly::IOBuf &) noexcept {
    FlatBufferBuilder builder;
    // auto msg = builder.CreateString( message );
    // auto response = CreateBlockFinalizeResponse( builder, msg );
    // builder.Finish( response );
    return string(
        reinterpret_cast<const char *>(builder.GetBufferPointer()), builder.GetSize());
}
