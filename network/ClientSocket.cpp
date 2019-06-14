/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skale-consensus.  If not, see <http://www.gnu.org/licenses/>.

    @file ClientSocket.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../SkaleCommon.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"

#include "../thirdparty/json.hpp"
#include "../chains/Schain.h"
#include "Sockets.h"

#include "../node/NodeInfo.h"
#include "../network/Connection.h"
#include "../exceptions/FatalError.h"
#include "../exceptions/NetworkProtocolException.h"
#include "../exceptions/ConnectionRefusedException.h"
#include "ClientSocket.h"

using namespace std;


void ClientSocket::closeSocket() {
    Connection::decrementTotalConnections();
    close((int) descriptor);

}


file_descriptor ClientSocket::getDescriptor() {
    return descriptor;
}

ptr<std::string> &ClientSocket::getConnectionIP() {
    return remoteIP;
}

network_port ClientSocket::getConnectionPort() {
    return remotePort;
}

ptr<sockaddr_in> ClientSocket::getSocketaddr() {
    return remote_addr;
}


int ClientSocket::createTCPSocket() {
    int s;

    if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        BOOST_THROW_EXCEPTION(FatalError("Could not create outgoing socket:" + string(strerror(errno))));
    }

    if (::bind(s, (struct sockaddr *) bind_addr.get(), sizeof(sockaddr_in)) < 0) {
        close(s);
        BOOST_THROW_EXCEPTION(FatalError("Could not bind socket address" + string(strerror(errno))));
    }

    // Init the connection
    if (connect(s, (sockaddr *) remote_addr.get(), sizeof(remote_addr)) < 0) {
        close(s);
        BOOST_THROW_EXCEPTION(ConnectionRefusedException("Could not connect to server", errno, __CLASS_NAME__));
    };


    Connection::incrementTotalConnections();

    return s;
}


ClientSocket::ClientSocket(Schain &_sChain, schain_index _destinationIndex, port_type portType)
        : bindIP(_sChain.getNode()->getBindIP()) {
    if (_sChain.getNode()->getNodeInfoByIndex(_destinationIndex) == nullptr) { // XXXX
        BOOST_THROW_EXCEPTION(FatalError("Could not find node with destination index "));
    }

    ptr<NodeInfo> ni = _sChain.getNode()->getNodeInfoByIndex(_destinationIndex); // XXXX


    remoteIP = ni->getBaseIP();
    remotePort = ni->getPort() + portType;


    this->remote_addr = Sockets::createSocketAddress(remoteIP, (uint16_t) remotePort);
    this->bind_addr = Sockets::createSocketAddress(bindIP, 0);


    descriptor = createTCPSocket();


    ASSERT(descriptor != 0);
}
