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

    @file TransportBiteBlockFinalizeAndDecryptServer.cpp
    @author Stan Kladko
    @date 2018-
*/


#include <flatbuffers/block_finalize_request_generated.h>
#include <flatbuffers/block_finalize_response_generated.h>
#include <flatbuffers/block_finalize_request_generated.h>
#include <flatbuffers/block_transactions_request_generated.h>
#include <flatbuffers/block_transactions_response_generated.h>


#include "SkaleCommon.h"
#include "chains/Schain.h"
#include <folly/SocketAddress.h>
#include "exceptions/FatalError.h"
#include "node/Node.h"


#include "exceptions/ExitRequestedException.h"
#include "exceptions/InvalidMessageFormatException.h"


#include "BlockFinalizeRequestHandler.h"
#include "BiteBlockFinalizeServer.h"


BiteBlockFinalizeServer::BiteBlockFinalizeServer(Schain &_sChain) : sChain(_sChain) {
    auto bindIP = _sChain.getNode()->getBindIP();
    auto port = sChain.getNode()->getBasePort() + port_type::BITE_SERVER;
    auto socketAddress = folly::SocketAddress(bindIP, (uint16_t) port, false);
    ipConfig = std::make_unique<HTTPServer::IPConfig>(socketAddress, HTTPServer::Protocol::HTTP2);
    auto threadFactory = std::make_shared<folly::NamedThreadFactory>("BlFinWorker");
    folly::IOThreadPoolExecutor workerThreadPool(1, std::thread::hardware_concurrency(),threadFactory);
}

BiteBlockFinalizeServer::~BiteBlockFinalizeServer() {
}

void BiteBlockFinalizeServer::start() {
    std::string server_address("0.0.0.0:50051");
    std::cout << "Server listening on " << server_address << std::endl;
}


