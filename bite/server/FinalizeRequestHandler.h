#pragma once

#include "proxygen/httpserver/RequestHandler.h"
#include "proxygen/httpserver/RequestHandlerFactory.h"
#include "proxygen/httpserver/HTTPServer.h"
#include "proxygen/httpserver/ResponseBuilder.h"
#include "folly/init/Init.h"
#include "folly/io/async/EventBaseManager.h"

#include "flatb/common_structures_generated.h"
#include "flatb/block_finalize_request_generated.h"
#include "flatb/block_finalize_response_generated.h"
#include "flatb/block_transactions_request_generated.h"
#include "flatb/block_transactions_response_generated.h"

#include "abstracttcpserver/ConnectionStatus.h"


using namespace proxygen;
using namespace flatbuffers;
using namespace skale_fb;


enum RequestType { BLOCK_FINALIZE, INVALID };

class FinalizeRequestHandler : public RequestHandler {
    RequestType requestType;
    folly::IOBufQueue bodyQueue{ folly::IOBufQueue::cacheChainLength() };  // Efficient buffer


public:
    void onRequest( std::unique_ptr< HTTPMessage > _headers ) noexcept override;

    void onBody( std::unique_ptr< folly::IOBuf > _body ) noexcept override;

    void onEOM() noexcept override;

    void onError( ProxygenError err ) noexcept override;

    void sendFlatBufferSuccessResponse( const std::string& response ) noexcept;

    void sendFlatBufferError(ConnectionSubStatus _substatus,std:: string _message);

    std::string getBlockFinalizeResponse( const folly::IOBuf& _request ) noexcept;

    std::string getBlockTransactionsResponse(const folly::IOBuf& _request) noexcept;

    void onUpgrade(proxygen::UpgradeProtocol)  noexcept override {};

    void requestComplete()  noexcept override {};


    void sendHTTPError(uint32_t _errorCode, const std::string& _message) const;

    void sendHTTPResponse(uint16_t _statusCode, const std::string& _statusMessage, const std::string& _body);
};

