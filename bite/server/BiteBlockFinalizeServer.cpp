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
#include "chains/Schain.h"

#include "BiteBlockFinalizeServer.h"


BiteBlockFinalizeServer::BiteBlockFinalizeServer(Schain &_sChain) : sChain(_sChain) {
    auto bindIP = _sChain.getNode()->getBindIP();
    auto port = sChain.getNode()->getBasePort() + port_type::BITE_SERVER;
    auto socketAddress = folly::SocketAddress(bindIP, (uint16_t) port, false);
    ipConfig = std::make_unique<proxygen::HTTPServer::IPConfig>(socketAddress, proxygen::HTTPServer::Protocol::HTTP2);
}

BiteBlockFinalizeServer::~BiteBlockFinalizeServer() {
}

void BiteBlockFinalizeServer::start() {
    std::string server_address("0.0.0.0:50051");
    std::cout << "Server listening on " << server_address << std::endl;
}


