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

    @file Sockets.h
    @author Stan Kladko
    @date 2018
*/

#pragma once




#include <arpa/inet.h>
#include <mutex>

#include <memory>
#include "node/Node.h"


class ServerSocker;
class TCPServerSocket;
class ZMQServerSocket;

class Sockets {

    Node& node;
public:
    Node &getNode() const;

public:
    Sockets(Node &node);

public:



    ptr<ZMQServerSocket> consensusZMQSocket = nullptr;

    ptr<ZMQServerSocket> getConsensusZMQSocket() const;


    ptr<TCPServerSocket> blockProposalSocket;

    ptr<TCPServerSocket> catchupSocket;


    static ptr<sockaddr_in> createSocketAddress(ptr<string> &ip, uint16_t port);

    void initSockets(ptr<string> &bindIP, uint16_t basePort);

};



