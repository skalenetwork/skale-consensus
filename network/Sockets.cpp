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

    @file Sockets.cpp
    @author Stan Kladko
    @date 2018
*/


#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"
#include "thirdparty/json.hpp"

#include "Network.h"
#include "Sockets.h"
#include "TCPServerSocket.h"
#include "ZMQSockets.h"

using namespace std;

void Sockets::initSockets( const string& _bindIP, uint16_t basePort, const SocketPortDeltas& ports ) {
    CHECK_ARGUMENT( !_bindIP.empty() );

    CONS_LOG( debug, "Initing network processing\n" );

    // ZMQ sockets use the base port plus lane delta for both local bind and remote connect.
    consensusZMQSockets =
        make_shared< ZMQSockets >( _bindIP, basePort + ports.ZMQBinaryConsensusPortDelta,
            static_cast< port_type >( ports.ZMQBinaryConsensusPortDelta ) );
    bulkDataZMQSockets =
        make_shared< ZMQSockets >( _bindIP, basePort + ports.ZMQBulkDataPortDelta,
            static_cast< port_type >( ports.ZMQBulkDataPortDelta ) );
    
    // TCP sockets used only for server side - no need to differentiate port types
    blockProposalSocket = make_shared< TCPServerSocket >( _bindIP, basePort + ports.TCPBlockProposalPortDelta );
    catchupSocket = make_shared< TCPServerSocket >( _bindIP, basePort + ports.TCPCatchupPortDelta );
}


ptr< sockaddr_in > Sockets::createSocketAddress( const string& _ip, uint16_t port ) {
    CHECK_ARGUMENT( !_ip.empty() )

    CHECK_STATE( Network::validateIpAddress( _ip ) > 0 );

    auto a = make_shared< struct sockaddr_in >();
    memset( a.get(), 0, sizeof( struct sockaddr_in ) );

    uint32_t ipbin = inet_addr( _ip.c_str() );

    a->sin_family = AF_INET;
    a->sin_port = htons( port );
    a->sin_addr.s_addr = ipbin;

    return a;
}

Sockets::Sockets( Node& node ) : node( node ) {}

Node& Sockets::getNode() const {
    return node;
}

ptr< ZMQSockets > Sockets::getConsensusZMQSockets() const {
    CHECK_STATE( consensusZMQSockets );
    return consensusZMQSockets;
}

ptr< ZMQSockets > Sockets::getBulkDataZMQSockets() const {
    CHECK_STATE( bulkDataZMQSockets );
    return bulkDataZMQSockets;
}
