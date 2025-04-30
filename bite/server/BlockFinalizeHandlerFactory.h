//
// Created by stan on 14-03-2025.
//

#pragma once

#include <proxygen/httpserver/RequestHandlerFactory.h>

class BlockFinalizeHandlerFactory : public proxygen::RequestHandlerFactory {
public:
    void onServerStart( folly::EventBase* ) noexcept override;

    proxygen::RequestHandler* onRequest(
        proxygen::RequestHandler*, proxygen::HTTPMessage* ) noexcept override;

    void onServerStop() noexcept override {}
};



