//
// Created by stan on 14-03-2025.
//

#include "FlatBufferRequestHandler.h"

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
        bodyQueue.append( std::move( _body ) );  // Zero-copy accumulation
    }
}

void FlatBufferRequestHandler::onEOM() noexcept {
    std::string responseMessage;
    std::string response;

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
    if ( request->length() < 16 ) {
        ResponseBuilder( downstream_ )
            .status( 400, "Bad Request" )
            .body( "Bad Request: Corrupt FlatBuffer: length " +
                   std::to_string( request->length() ) )
            .sendWithEOM();
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

    sendFlatBufferResponse( response );
}
void FlatBufferRequestHandler::sendHTTPError(uint32_t _errorCode, const std::string& _message) const {
    ResponseBuilder( downstream_ )
        .status( _errorCode, _message )
        .body( _message)
        .sendWithEOM();
}

void FlatBufferRequestHandler::onError( ProxygenError err ) noexcept {
    std::cerr << "Error: " << proxygen::getErrorString( err ) << std::endl;
}

void FlatBufferRequestHandler::sendFlatBufferResponse( const std::string& response )  noexcept{
    ResponseBuilder( downstream_ )
        .status( 200, "OK" )
        .header( "Content-Type", "application/octet-stream" )
        .body( response )
        .sendWithEOM();
}


std::string FlatBufferRequestHandler::getBlockFinalizeResponse(
    const folly::IOBuf& _request ) noexcept {
    FlatBufferBuilder builder;
    // auto msg = builder.CreateString( message );
    // auto response = CreateBlockFinalizeResponse( builder, msg );
    // builder.Finish( response );
    return std::string(
        reinterpret_cast< const char* >( builder.GetBufferPointer() ), builder.GetSize() );
}


std::string FlatBufferRequestHandler::getBlockTransactionsResponse (
    const folly::IOBuf& _request) noexcept {
    FlatBufferBuilder builder;
    // auto msg = builder.CreateString( message );
    // auto response = CreateBlockFinalizeResponse( builder, msg );
    // builder.Finish( response );
    return std::string(
        reinterpret_cast< const char* >( builder.GetBufferPointer() ), builder.GetSize() );
}
