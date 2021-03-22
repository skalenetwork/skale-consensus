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

    @file TCPServerSocket.cpp
    @author Stan Kladko
    @date 2018
*/

#include <boost/asio.hpp>

#include "Log.h"
#include "SkaleCommon.h"

#include "exceptions/FatalError.h"

#include "Network.h"
#include "Sockets.h"
#include "TCPServerSocket.h"

TCPServerSocket::TCPServerSocket(const string& _bindIP, uint16_t _basePort, port_type _portType )
    : ServerSocket( _bindIP, _basePort, _portType ) {

    socketaddr = Sockets::createSocketAddress( bindIP, bindPort );

    CHECK_STATE(socketaddr);

    LOG( debug, "Creating TCP listen socket" );

    int s;

    if ( ( s = socket( AF_INET, SOCK_STREAM, 0 ) ) < 0 ) {
        BOOST_THROW_EXCEPTION( FatalError( "Could not create read socket" ) );
    }

    int iSetOption = 1;

    setsockopt( s, SOL_SOCKET, SO_REUSEADDR, ( char* ) &iSetOption, sizeof( iSetOption ) );

    if ( ::bind( s, ( struct sockaddr* ) socketaddr.get(), sizeof( sockaddr_in ) ) < 0 ) {
        BOOST_THROW_EXCEPTION(
            FatalError( "Could not bind the TCP socket on address:" + _bindIP +
           ":" + to_string(bindPort) + " error: " + to_string( errno ) ) );
    }

    // Init the connection
    listen( s, SOCKET_BACKLOG );

    LOG( debug, "Successfully created TCP listen socket" );

    descriptor = s;

    CHECK_STATE( descriptor > 0 );
}


TCPServerSocket::~TCPServerSocket() {
    if ( descriptor != 0 )
        close( descriptor );
}

void TCPServerSocket::touch() {
    try {
        using namespace boost::asio;
        ip::tcp::endpoint ep( ip::address::from_string( bindIP ), bindPort );
        io_service service;
        ip::tcp::socket sock( service );
        sock.connect( ep );
    } catch (...) {};    
}

int TCPServerSocket::getDescriptor() {
    CHECK_STATE(descriptor);
    return descriptor;
}


void TCPServerSocket::closeAndCleanupAll() {
    auto previous = descriptor.exchange(0);
    if ( previous != 0 ) {
        close( descriptor );
    }
}
