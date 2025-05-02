//
// Created by stan on 14-03-2025.
//

#include "FlatBufferRequestHandler.h"
#include "BlockFinalizeHandlerFactory.h"

void BlockFinalizeHandlerFactory::onServerStart( folly::EventBase* ) noexcept {}

proxygen::RequestHandler* BlockFinalizeHandlerFactory::onRequest(
    proxygen::RequestHandler*, proxygen::HTTPMessage* ) noexcept {
    return new FlatBufferRequestHandler();
}
