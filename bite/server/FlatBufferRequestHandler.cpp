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

    // Merge chained buffers into a contiguous block (efficient if already contiguous)
    auto coalescedBody = bodyQueue.move()->coalesce();

    switch ( requestType ) {
    case RequestType::BLOCK_FINALIZE: {
        auto blockFinalizeRequest = GetBlockFinalizeRequest( coalescedBody.data() );
        response = getBlockFinalizeResponse( blockFinalizeRequest );
        break;
    }
    case RequestType::BLOCK_TXS: {
        auto blockTransactionsRequest = GetBlockTransactionsRequest( coalescedBody.data() );
        response = getBlockTransactionsResponse( blockTransactionsRequest );
        break;
    }
    default:
        sendErrorResponse();
        return;
    }

    sendFlatBufferResponse( response );
}

void FlatBufferRequestHandler::onError( ProxygenError err ) noexcept {
    std::cerr << "Error: " << proxygen::getErrorString( err ) << std::endl;
}

void FlatBufferRequestHandler::sendFlatBufferResponse( const std::string& response ) {
    ResponseBuilder( downstream_ )
        .status( 200, "OK" )
        .header( "Content-Type", "application/octet-stream" )
        .body( response )
        .sendWithEOM();
}

void FlatBufferRequestHandler::sendErrorResponse() {
    ResponseBuilder( downstream_ )
        .status( 404, "Not Found" )
        .body( "Unknown endpoint" )
        .sendWithEOM();
}


std::string FlatBufferRequestHandler::getBlockFinalizeResponse(
    const BlockFinalizeRequest* _request ) {
    FlatBufferBuilder builder;
    // auto msg = builder.CreateString( message );
    // auto response = CreateBlockFinalizeResponse( builder, msg );
    // builder.Finish( response );
    return std::string(
        reinterpret_cast< const char* >( builder.GetBufferPointer() ), builder.GetSize() );
}


std::string FlatBufferRequestHandler::getBlockTransactionsResponse(
    const BlockTransactionsRequest* _request ) {
    FlatBufferBuilder builder;
    // auto msg = builder.CreateString( message );
    // auto response = CreateBlockFinalizeResponse( builder, msg );
    // builder.Finish( response );
    return std::string(
        reinterpret_cast< const char* >( builder.GetBufferPointer() ), builder.GetSize() );
}
