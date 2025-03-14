//
// Created by stan on 14-03-2025.
//

#ifndef SKALED_FLATBUFFERREQUESTRESPONSEHANDLER_H
#define SKALED_FLATBUFFERREQUESTRESPONSEHANDLER_H


#include "proxygen/httpserver/RequestHandler.h"
#include "proxygen/httpserver/RequestHandlerFactory.h"
#include "proxygen/httpserver/HTTPServer.h"
#include "proxygen/httpserver/ResponseBuilder.h"
#include "folly/init/Init.h"
#include "folly/io/async/EventBaseManager.h"

#include "flatbuffers/block_finalize_common_structures_generated.h"
#include "flatbuffers/block_finalize_request_generated.h"
#include "flatbuffers/block_finalize_response_generated.h"
#include "flatbuffers/block_transactions_request_generated.h"
#include "flatbuffers/block_transactions_response_generated.h"


using namespace proxygen;
using namespace flatbuffers;
using namespace block_finalize;


enum RequestType { BLOCK_FINALIZE, BLOCK_TXS, INVALID };

class FlatBufferHandler : public RequestHandler {
    RequestType requestType;
    folly::IOBufQueue bodyQueue{ folly::IOBufQueue::cacheChainLength() };  // Efficient buffer


public:
    void onRequest( std::unique_ptr< HTTPMessage > _headers ) noexcept override {
        auto requestPath = _headers->getPath();
        if ( requestPath == "/block_finalize" ) {
            requestType = RequestType::BLOCK_FINALIZE;
        } else if ( requestPath == "/block_txs" ) {
            requestType = RequestType::BLOCK_TXS;
        } else {
            requestType = RequestType::INVALID;
        }
    }

    void onBody( std::unique_ptr< folly::IOBuf > _body ) noexcept override {
        // HTTP2 may send body in chunks, so we are accumulating
        if ( _body ) {
            bodyQueue.append( std::move( _body ) );  // Zero-copy accumulation
        }
    }

    void onEOM() noexcept override {
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

    void onError( ProxygenError err ) noexcept override {
        std::cerr << "Error: " << proxygen::getErrorString( err ) << std::endl;
    }

    void sendFlatBufferResponse( const std::string& response ) {
        ResponseBuilder( downstream_ )
            .status( 200, "OK" )
            .header( "Content-Type", "application/octet-stream" )
            .body( response )
            .sendWithEOM();
    }

    void sendErrorResponse() {
        ResponseBuilder( downstream_ )
            .status( 404, "Not Found" )
            .body( "Unknown endpoint" )
            .sendWithEOM();
    }


    std::string getBlockFinalizeResponse( const BlockFinalizeRequest* _request ) {
        FlatBufferBuilder builder;
        // auto msg = builder.CreateString( message );
        // auto response = CreateBlockFinalizeResponse( builder, msg );
        // builder.Finish( response );
        return std::string(
            reinterpret_cast< const char* >( builder.GetBufferPointer() ), builder.GetSize() );
    }


    std::string getBlockTransactionsResponse( const BlockTransactionsRequest* _request ) {
        FlatBufferBuilder builder;
        // auto msg = builder.CreateString( message );
        // auto response = CreateBlockFinalizeResponse( builder, msg );
        // builder.Finish( response );
        return std::string(
            reinterpret_cast< const char* >( builder.GetBufferPointer() ), builder.GetSize() );
    }
};


#endif  // SKALED_FLATBUFFERREQUESTRESPONSEHANDLER_H
