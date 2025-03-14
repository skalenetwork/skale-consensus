//
// Created by stan on 14-03-2025.
//

#ifndef SKALED_BLOCKFINALIZEHANDLERFACTORY_H
#define SKALED_BLOCKFINALIZEHANDLERFACTORY_H

#include <proxygen/httpserver/RequestHandlerFactory.h>

class BlockFinalizeHandlerFactory : public proxygen::RequestHandlerFactory {
public:
    void onServerStart( folly::EventBase* ) noexcept override;

    proxygen::RequestHandler* onRequest(
        proxygen::RequestHandler*, proxygen::HTTPMessage* ) noexcept override;

    void onServerStop() noexcept override {}
};


#endif  // SKALED_BLOCKFINALIZEHANDLERFACTORY_H
