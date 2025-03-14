//
// Created by stan on 14-03-2025.
//

#ifndef SKALED_FLATBUFFERREQUESTHANDLER_H
#define SKALED_FLATBUFFERREQUESTHANDLER_H


#include "proxygen/httpserver/RequestHandler.h"
#include "proxygen/httpserver/RequestHandlerFactory.h"
#include "proxygen/httpserver/HTTPServer.h"
#include "proxygen/httpserver/ResponseBuilder.h"
#include "folly/init/Init.h"
#include "folly/io/async/EventBaseManager.h"

#include "flatb/block_finalize_common_structures_generated.h"
#include "flatb/block_finalize_request_generated.h"
#include "flatb/block_finalize_response_generated.h"
#include "flatb/block_transactions_request_generated.h"
#include "flatb/block_transactions_response_generated.h"


using namespace proxygen;
using namespace flatbuffers;
using namespace block_finalize;


enum RequestType { BLOCK_FINALIZE, BLOCK_TXS, INVALID };

class FlatBufferRequestHandler : public RequestHandler {
    RequestType requestType;
    folly::IOBufQueue bodyQueue{ folly::IOBufQueue::cacheChainLength() };  // Efficient buffer


public:
    void onRequest( std::unique_ptr< HTTPMessage > _headers ) noexcept override;

    void onBody( std::unique_ptr< folly::IOBuf > _body ) noexcept override;

    void onEOM() noexcept override;

    void onError( ProxygenError err ) noexcept override;

    void sendFlatBufferResponse( const std::string& response ) noexcept;

    std::string getBlockFinalizeResponse( const folly::IOBuf& _request ) noexcept;

    std::string getBlockTransactionsResponse(const folly::IOBuf& _request) noexcept;

    void onUpgrade(proxygen::UpgradeProtocol prot)  noexcept override {};

    void requestComplete()  noexcept override {};


    void sendHTTPError(uint32_t _errorCode, const std::string& _message) const;
};


#endif  // SKALED_FLATBUFFERREQUESTHANDLER_H
