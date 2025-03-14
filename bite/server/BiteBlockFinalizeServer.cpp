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


#include <flatbuffers/block_finalize_request_generated.h>
#include <flatbuffers/block_finalize_response_generated.h>
#include <flatbuffers/block_finalize_request_generated.h>
#include <flatbuffers/block_transactions_request_generated.h>
#include <flatbuffers/block_transactions_response_generated.h>


#include "SkaleCommon.h"

#include <threads/GlobalThreadRegistry.h>
#include "chains/Schain.h"

#include "BiteBlockFinalizeServer.h"

BiteBlockFinalizeServer::BiteBlockFinalizeServer(Schain &_sChain) : sChain(_sChain) {
    auto bindIP = _sChain.getNode()->getBindIP();
    auto port = sChain.getNode()->getBasePort() + port_type::BITE_SERVER;
    auto socketAddress = folly::SocketAddress(bindIP, (uint16_t) port, false);
    proxygen::HTTPServer::IPConfig ipConfig(socketAddress, proxygen::HTTPServer::Protocol::HTTP2);

    // Use a dynamic thread pool instead of a fixed thread count
    auto maxThreads = std::thread::hardware_concurrency();
    auto ioExecutor = std::make_shared<folly::IOThreadPoolExecutor>(
        1,  // Min threads
        maxThreads,  // Max threads
        std::chrono::seconds(10)  // Threads are removed if idle for 10 sec
    );



    proxygen::HTTPServerOptions options;
    options.threads = 8;
    options.idleTimeout = std::chrono::milliseconds(60000);
    options.enableContentCompression = false;
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

void BiteBlockFinalizeServer::exitProxygenServer() {
    if (!runServerThread) {
        return;
    }
    this->proxygenServerInstance->stopListening();  // Stop new connections
    // give on-going requests a little time to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    this->proxygenServerInstance->stop();  // Shutdown completely
    this->runServerThread->join();
}


void BiteBlockFinalizeServer::runServer() {
    CHECK(proxygenServerInstance);
    try {
        proxygenServerInstance->start();
    } catch (const std::exception& e) {
        ConsensusEngine::log(critical, string("Proxygen exception: ") + e.what(), __CLASS_NAME__);
        throw;
    } catch (...) {
        ConsensusEngine::log(critical,  "Unknown exception occurred!",  __CLASS_NAME__);
        throw;
    }
}

