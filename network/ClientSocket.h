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

    @file ClientSocket.h
    @author Stan Kladko
    @date 2018
*/

#pragma once

#include "Socket.h"

class Node;
class NodeInfo;
class Schain;

class ClientSocket {
    static atomic< int64_t > totalSockets;

    Socket descriptor;

    string remoteIP;

    network_port remotePort = 0;

    static Socket createTCPSocket();

    void connectTCPSocket( const Socket& _socket, const sockaddr_in& _remoteAddress );

public:
    file_descriptor getDescriptor();

    string& getIP();

    network_port getPort();

    static uint64_t getTotalSockets();

    virtual ~ClientSocket() {
        totalSockets--;
    }

    ClientSocket( Schain& _sChain, schain_index _destinationIndex, port_type portType );
};
