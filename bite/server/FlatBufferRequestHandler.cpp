


#include "abstracttcpserver/ConnectionStatus.h"

#include "FlatBufferRequestHandler.h"
#undef LOG // avoid macro definition  conflicts with proxygen LOG
#include "Log.h"
#include "flatb/ErrorResponseObject.h"

using namespace std;

constexpr uint64_t MIN_FLATBUFFER_REQUEST_LEN = 16;

void FlatBufferRequestHandler::onRequest( std::unique_ptr< HTTPMessage > _headers ) noexcept {
    auto requestPath = _headers->getPath();
    if ( requestPath == "/block_finalize" ) {
        requestType = RequestType::BLOCK_FINALIZE;
    } else if ( requestPath == "/block_txs" ) {
        requestType = RequestType::BLOCK_TXS;
    } else {
        requestType = RequestType::INVALID;
    }
}

void FlatBufferRequestHandler::onBody( std::unique_ptr< folly::IOBuf > _body ) noexcept {
    // HTTP2 may send body in chunks, so we are accumulating
    if ( _body ) {
        bodyQueue.append( move( _body ) );  // Zero-copy accumulation
    }
}



void FlatBufferRequestHandler::onEOM() noexcept {
    string responseMessage;
    string response;

    try {
        if ( requestType == INVALID ) {
            ResponseBuilder( downstream_ )
                .status( 404, "Not Found" )
                .body( "Unknown endpoint" )
                .sendWithEOM();
            return;
        }

        // Merge chained buffers into a contiguous block (efficient if already contiguous)
        auto request = bodyQueue.move();
        request->coalesce();

        // do sanity check
        if ( request->empty()) {
            ResponseBuilder( downstream_ )
                .status( 400, "Bad Request" )
                .body( "Bad Request: Empty FlatBuffer data" )
                .sendWithEOM();
            return;
        }

        // Check for minimum valid FlatBuffer length (assumed 16 bytes)
        if ( request->length() < MIN_FLATBUFFER_REQUEST_LEN ) {
            sendFlatBufferError(ConnectionSubStatus::CONNECTION_ERROR_CANT_PARSE_REQUEST,
                "FlatBuffer request too short too parse: length: " + to_string( request->length() ) );
            return;
        }

        // Due to flatbuffer format the fourth byte is always zero
        if ( request->data()[4] != 0 ) {
            sendHTTPError(400, "Bad Request: Corrupt FlatBuffer");
            return;
        }


        try {
            switch ( requestType ) {
                case RequestType::BLOCK_FINALIZE: {
                    response = getBlockFinalizeResponse( *request );
                    break;
                }
                case RequestType::BLOCK_TXS: {
                    auto blockTransactionsRequest = getBlockTransactionsResponse( *request );
                    break;
                }
                default:
                    // we should never get here
                        ResponseBuilder( downstream_ )
                            .status( 500, "Internal Server Error" )
                            .body( "Internal Server Error" )
                            .sendWithEOM();
                return;
            }
        } catch (Exception& _e) {
            //TODO
        } catch (...) {
            //TODO
        }

        sendFlatBufferSuccessResponse( response );
    } CATCH_AND_LOG_ANY_EXCEPTION(error, "Error processing FlatBuffer request");
}
void FlatBufferRequestHandler::sendHTTPError(uint32_t _errorCode, const string& _message) const {
    ResponseBuilder( downstream_ )
        .status( _errorCode, _message )
        .body( _message)
        .sendWithEOM();
}

void FlatBufferRequestHandler::sendHTTPResponse(uint16_t _statusCode, const string& _statusMessage,
    const string& _body) {

    ResponseBuilder( downstream_ )
        .status( _statusCode, _statusMessage )
        .header( "Content-Type", "application/octet-stream" )
        .body( _body)
        .sendWithEOM();
}



void FlatBufferRequestHandler::onError( ProxygenError _err ) noexcept {
    LOG(err, "Error in FlatBufferRequestHandler: " + to_string( _err ));
}

void FlatBufferRequestHandler::sendFlatBufferSuccessResponse( const string& response )  noexcept{
    sendHTTPResponse(200, response, "OK" );
}

void FlatBufferRequestHandler::sendFlatBufferError(ConnectionSubStatus _substatus, string _message) {
    ErrorResponseObject ero(ConnectionStatus::CONNECTION_ERROR, _substatus, 0, 0, 0,
        _message);

    auto fbError = ero.serialize();

    ResponseBuilder( downstream_ )
            .status( 400, _message  )
            .body( *fbError )
            .sendWithEOM();
}


string FlatBufferRequestHandler::getBlockFinalizeResponse(
    const folly::IOBuf& _request ) noexcept {
    FlatBufferBuilder builder;
    // auto msg = builder.CreateString( message );
    // auto response = CreateBlockFinalizeResponse( builder, msg );
    // builder.Finish( response );
    return string(
        reinterpret_cast< const char* >( builder.GetBufferPointer() ), builder.GetSize() );
}


string FlatBufferRequestHandler::getBlockTransactionsResponse (
    const folly::IOBuf& _request) noexcept {
    FlatBufferBuilder builder;
    // auto msg = builder.CreateString( message );
    // auto response = CreateBlockFinalizeResponse( builder, msg );
    // builder.Finish( response );
    return string(
        reinterpret_cast< const char* >( builder.GetBufferPointer() ), builder.GetSize() );
}
