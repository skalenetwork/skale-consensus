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
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/tcp.h>
#include <unistd.h>
#include <utility>

#include "Log.h"
#include "SkaleCommon.h"

#include "exceptions/FatalError.h"

#include "Network.h"
#include "ServerConnection.h"
#include "Sockets.h"
#include "Socket.h"
#include "TCPServerSocket.h"
#include "tests/TestHooks.h"

namespace {
bool isRetryableAcceptError( int error ) {
    // Interrupted calls and Linux pending-connection network errors allow another accept attempt.
    switch ( error ) {
        case EINTR:
        case ECONNABORTED:
        case ENETDOWN:
        case EPROTO:
        case ENOPROTOOPT:
        case EHOSTDOWN:
        case ENONET:
        case EHOSTUNREACH:
        case EOPNOTSUPP:
        case ENETUNREACH:
            return true;
        default:
            return false;
    }
}

bool isAcceptResourceError( int error ) {
    // Descriptor or kernel-memory exhaustion needs backoff to avoid a busy retry loop.
    return error == EMFILE || error == ENFILE || error == ENOBUFS || error == ENOMEM;
}
} // namespace

TCPServerSocket::AcceptResult TCPServerSocket::acceptConnection() {
    sockaddr_in clientAddress{};
    socklen_t addressLength = sizeof( clientAddress );
    const int listener = getDescriptor();

    // ------ test only hook for injecting accept errors ------
    int injectedError = 0;
    TestHooks::TCPAcceptErrorHook::fire( *this, injectedError );
    CHECK_STATE( injectedError >= 0 );
    // --------------------------------------------------------

    const int accepted = injectedError == 0 ?
        ::accept( listener, reinterpret_cast< sockaddr* >( &clientAddress ), &addressLength ) : -1;
    if ( accepted < 0 ) {
        const int error = injectedError != 0 ? injectedError : errno;
        if ( isRetryableAcceptError( error ) )
            return { AcceptStatus::RetryableError, nullptr, error };
        if ( isAcceptResourceError( error ) )
            return { AcceptStatus::ResourceExhausted, nullptr, error };
        return { AcceptStatus::FatalError, nullptr, error };
    }

    Socket acceptedSocket( accepted );

    // Disable Nagle's algorithm to reduce latency for small TCP writes.
    const int enableTcpNoDelay = 1;
    if ( setsockopt( accepted, IPPROTO_TCP, TCP_NODELAY,
             &enableTcpNoDelay, sizeof( enableTcpNoDelay ) ) != 0 ) {
        const int error = errno;
        BOOST_THROW_EXCEPTION( FatalError(
            "Could not configure accepted TCP socket: " + string( strerror( error ) ) ) );
    }

    char ip[INET_ADDRSTRLEN];
    if ( !inet_ntop( AF_INET, &clientAddress.sin_addr, ip, sizeof( ip ) ) ) {
        const int error = errno;
        BOOST_THROW_EXCEPTION( FatalError(
            "Could not format TCP peer address: " + string( strerror( error ) ) ) );
    }

    auto connection = make_shared< ServerConnection >( std::move( acceptedSocket ), ip );
    return { AcceptStatus::Accepted, std::move( connection ), 0 };
}

TCPServerSocket::TCPServerSocket( const string& _bindIP, uint16_t _basePort, port_type _portType )
    : ServerSocket( _bindIP, _basePort, _portType ), portType( _portType ) {
    socketaddr = Sockets::createSocketAddress( bindIP, bindPort );

    CHECK_STATE( socketaddr );

    CONS_LOG( debug, "Creating TCP listen socket" );

    int s;

    if ( ( s = socket( AF_INET, SOCK_STREAM, 0 ) ) < 0 ) {
        BOOST_THROW_EXCEPTION( FatalError( "Could not create read socket" ) );
    }
    descriptor = Socket{ s };

    int iSetOption = 1;

    if ( setsockopt( s, SOL_SOCKET, SO_REUSEADDR,
             ( char* ) &iSetOption, sizeof( iSetOption ) ) < 0 ) {
        BOOST_THROW_EXCEPTION( FatalError(
            "Could not set TCP listener SO_REUSEADDR: " + string( strerror( errno ) ) ) );
    }

    if ( ::bind( s, ( struct sockaddr* ) socketaddr.get(), sizeof( sockaddr_in ) ) < 0 ) {
        BOOST_THROW_EXCEPTION(
            FatalError( "Could not bind the TCP socket on address:" + _bindIP + ":" +
                        to_string( bindPort ) + " error: " + to_string( errno ) ) );
    }

    // Init the connection
    if ( listen( s, SOCKET_BACKLOG ) < 0 ) {
        BOOST_THROW_EXCEPTION( FatalError(
            "Could not listen on TCP socket: " + string( strerror( errno ) ) ) );
    }

    CONS_LOG( debug, "Successfully created TCP listen socket" );

    CHECK_STATE( descriptor.get() > 0 );
}


TCPServerSocket::~TCPServerSocket() = default;

void TCPServerSocket::touch() {
    try {
        using namespace boost::asio;
        ip::tcp::endpoint ep( ip::make_address( bindIP ), bindPort );
        io_context service;
        ip::tcp::socket sock( service );
        sock.connect( ep );
    } catch ( ... ) {
    };
}

int TCPServerSocket::getDescriptor() {
    LOCK( m )
    CHECK_STATE( descriptor );
    return descriptor.get();
}


void TCPServerSocket::closeAndCleanupAll() {
    Socket previous;
    {
        LOCK( m )
        previous = std::move( descriptor );
    }
    const int closeError = previous.reset();
    if ( closeError != 0 )
        CONS_LOG( err, "Could not close TCP listen socket: " << strerror( closeError ) );
}
