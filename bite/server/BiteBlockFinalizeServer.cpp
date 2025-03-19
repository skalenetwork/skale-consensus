/*
    Copyright (C) 2018- SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with skale-consensus.  If not, see <https://www.gnu.org/licenses/>.


    @author Stan Kladko
    @date 2025-
*/


// avoid macro definition conflicts with proxygen LOG


#include <flatb/block_finalize_request_generated.h>
#include <flatb/block_finalize_response_generated.h>
#include <flatb/block_transactions_request_generated.h>
#include <flatb/block_transactions_response_generated.h>
#include <proxygen/httpserver/HTTPServer.h>
#include "BlockFinalizeHandlerFactory.h"
#undef LOG // avoid macro definition conflicts with proxygen LOG
#undef CHECK // avoid macro definition conflicts with proxygen CHECK

#include "SkaleCommon.h"
#include "Log.h"
#include <threads/GlobalThreadRegistry.h>
#include "chains/Schain.h"
#include "node/Node.h"


#include "BiteBlockFinalizeServer.h"

BiteBlockFinalizeServer::BiteBlockFinalizeServer(Schain &_sChain) : sChain(_sChain) {
    auto bindIP = _sChain.getNode()->getBindIP();
    auto port = sChain.getNode()->getBasePort() + port_type::BITE_SERVER;
    auto socketAddress = folly::SocketAddress(bindIP, (uint16_t) port, false);
    proxygen::HTTPServer::IPConfig ipConfig(socketAddress, proxygen::HTTPServer::Protocol::HTTP2);

    proxygen::HTTPServerOptions options;
    options.threads = 8;
    options.handlerFactories = proxygen::RequestHandlerChain().addThen<BlockFinalizeHandlerFactory>().build();
    proxygenServerInstance = make_unique<proxygen::HTTPServer>(std::move(options));
    proxygenServerInstance->bind({ipConfig});
}

BiteBlockFinalizeServer::~BiteBlockFinalizeServer() {
}

void BiteBlockFinalizeServer::startProxygenServer() {
    CHECK_STATE(!runServerThread);
    runServerThread = std::make_unique<std::thread>(&BiteBlockFinalizeServer::runServer, this);
    // make thread  joined on exit
    sChain.getThreadRegistry()->add(runServerThread);
}


void BiteBlockFinalizeServer::runServer() {
    CHECK_STATE(proxygenServerInstance);
    try {
        LOG(info, "Starting Proxygen server");
        proxygenServerInstance->start();
        LOG(info, "Proxygen server started");
    } CATCH_AND_LOG_ANY_EXCEPTION(critical, "Exception in proxygen start");
}

void BiteBlockFinalizeServer::exitProxygenServer() noexcept {
    if (!runServerThread) {
        return;
    }

    LOG(info, "Exiting Proxygen server");

    try {
        proxygenServerInstance->stopListening(); // Stop new connection
    } CATCH_AND_LOG_ANY_EXCEPTION(critical, "Exception in proxygen stopListening");

    LOG(info, "Proxygen server stopped listening");
    // give on-going requests a little time to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    try {
        proxygenServerInstance->stop(); // Shutdown completely
    } CATCH_AND_LOG_ANY_EXCEPTION(critical, "Exception in proxygen stopListening");

    LOG(info, "Proxygen server stopped");
}



