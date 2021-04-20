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

    @file ClientSocket.cpp
    @author Stan Kladko
    @date 2018
*/

#include "Log.h"
#include "SkaleCommon.h"
#include "exceptions/FatalError.h"

#include "Sockets.h"
#include "chains/Schain.h"
#include "thirdparty/json.hpp"

#include "ClientSocket.h"
#include "exceptions/ConnectionRefusedException.h"
#include "node/NodeInfo.h"

using namespace std;


void ClientSocket::closeSocket() {
    LOCK( m )
    if ( descriptor != 0 )
        close( ( int ) descriptor );
    descriptor = 0;
}


file_descriptor ClientSocket::getDescriptor() {
    return descriptor;
}

string& ClientSocket::getIP() {
    CHECK_STATE(!remoteIP.empty() )
    return remoteIP;
}

network_port ClientSocket::getPort() {
    return remotePort;
}

int ClientSocket::createTCPSocket() {
    int s;

    if ( ( s = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ) < 0 ) {
        BOOST_THROW_EXCEPTION(
            FatalError( "Could not create outgoing socket:" + string( strerror( errno ) ) ) );
    }


    // Init the connection
    CHECK_STATE(remoteAddr)

    if ( connect( s, ( sockaddr* ) remoteAddr.get(), sizeof( remoteAddr ) ) < 0 ) {
        close( s );
        BOOST_THROW_EXCEPTION( ConnectionRefusedException(
            "Couldnt connect to:" + getIP() + ":" + to_string( getPort() ),
            errno, __CLASS_NAME__ ) );
    }

    return s;
}


ClientSocket::ClientSocket( Schain& _sChain, schain_index _destinationIndex, port_type portType ) {

    if ( _sChain.getNode()->getNodeInfoByIndex( _destinationIndex ) == nullptr ) {
        BOOST_THROW_EXCEPTION( FatalError( "Could not find node with destination index " ) );
    }

    ptr< NodeInfo > ni = _sChain.getNode()->getNodeInfoByIndex( _destinationIndex );

    CHECK_STATE(ni)

    remoteIP = ni->getBaseIP();

    CHECK_STATE(!remoteIP.empty())

    remotePort = ni->getPort() + portType;

    this->remoteAddr = Sockets::createSocketAddress( remoteIP, ( uint16_t ) remotePort );
    CHECK_STATE(remoteAddr)

    descriptor = createTCPSocket();

    CHECK_STATE( descriptor != 0 )

    totalSockets++;
}

atomic< int64_t > ClientSocket::totalSockets = 0;

uint64_t ClientSocket::getTotalSockets() {
    return totalSockets;
}
