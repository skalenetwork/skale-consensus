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

    @file ServerSocket.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"
#include "thirdparty/json.hpp"

#include "ServerSocket.h"
#include "Sockets.h"
#include "Utils.h"


ServerSocket::ServerSocket(ptr<string> &_bindIP, uint16_t _basePort, port_type _portType)
    : bindIP( _bindIP ) {

    bindPort = _basePort + _portType;

    ASSERT( Utils::isValidIpAddress( _bindIP ) );

    this->socketaddr = Sockets::createSocketAddress( bindIP, bindPort );

    LOG(debug, "Binding ip: " + *_bindIP + " " + to_string(bindPort) + " " +
               to_string(_basePort));

}

int ServerSocket::getDescriptor() {
    return descriptor;
}

ptr< std::string >& ServerSocket::getBindIP() {
    return bindIP;
}

uint32_t ServerSocket::getBindPort() {
    return bindPort;
}




ServerSocket::~ServerSocket() {
    closeSocket();
}


void ServerSocket::closeSocket() {
    close( descriptor );
}
