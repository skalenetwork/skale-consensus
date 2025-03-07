/*
    Copyright (C) 2018-2019 SKALE Labs

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

    @file TransportNetwork.h
    @author Stan Kladko
    @date 2018
*/

#pragma once

#include <grpcpp/grpcpp.h>

#include "Agent.h"

class Schain;
class Node;

class BlockFinalizeServiceImpl;

class BiteBlockFinalizeServer {

    std::unique_ptr<BlockFinalizeServiceImpl> service;
    std::unique_ptr<grpc::Server>  grpcServer;


public:
    explicit BiteBlockFinalizeServer(Schain &_sChain);

    void start();

    ~BiteBlockFinalizeServer();


};






