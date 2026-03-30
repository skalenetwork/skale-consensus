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

    @file ZMQSockets.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"

#include "ZMQSockets.h"
#include "node/NodeInfo.h"
#include "exceptions/FatalError.h"
#include "exceptions/ExitRequestedException.h"
#include "zmq.h"

ZMQSockets::ZMQSockets( const string& _bindIP, uint16_t _bindPort, port_type _portType )
    : ServerSocket( _bindIP, _bindPort ), portType( _portType ) {
    CHECK_ARGUMENT( !_bindIP.empty() )
    context = zmq_ctx_new();
}

void* ZMQSockets::getDestinationSocket( const ptr< NodeInfo >& _remoteNodeInfo ) {
    CHECK_ARGUMENT( _remoteNodeInfo );
    LOCK( m )

    // Exiting - throw exception
    if ( closeAndCleanupAllCalled ) {
        throw ExitRequestedException( __FUNCTION__ );
    }


    auto ipAddress = _remoteNodeInfo->getBaseIP();
    auto basePort = _remoteNodeInfo->getPort();

    auto schainIndex = _remoteNodeInfo->getSchainIndex();

    if ( sendSockets.count( schainIndex ) > 0 ) {
        return sendSockets.at( schainIndex );
    }

    void* requester = zmq_socket( context, ZMQ_CLIENT );

    CHECK_STATE2( requester, "Could not create ZMQ send socket" );

    int val = CONSENSUS_ZMQ_HWM;
    auto rc = zmq_setsockopt( requester, ZMQ_RCVHWM, &val, sizeof( val ) );
    CHECK_STATE( rc == 0 );
    rc = zmq_setsockopt( requester, ZMQ_SNDHWM, &val, sizeof( val ) );
    CHECK_STATE( rc == 0 );

    CHECK_STATE( requester );

    CONS_LOG( debug,
        getThreadName() << " zmq debug: requester = " << to_string( ( uint64_t ) requester ) );

    int timeout = ZMQ_TIMEOUT;
    int linger = 0;

    zmq_setsockopt( requester, ZMQ_SNDTIMEO, &timeout, sizeof( int ) );
    zmq_setsockopt( requester, ZMQ_RCVTIMEO, &timeout, sizeof( int ) );
    zmq_setsockopt( requester, ZMQ_LINGER, &linger, sizeof( int ) );


    int result = zmq_connect(
        requester, ( "tcp://" + ipAddress + ":" + to_string( basePort + portType ) ).c_str() );
    CONS_LOG( debug, "Connected ZMQ socket" << to_string( result ) );
    sendSockets[schainIndex] = requester;

    return requester;
}

void* ZMQSockets::getReceiveSocket() {
    LOCK( m )

    // Exiting - throw exception
    if ( closeAndCleanupAllCalled ) {
        throw ExitRequestedException( __FUNCTION__ );
    }

    if ( !receiveSocket ) {
        receiveSocket = zmq_socket( context, ZMQ_SERVER );

        CHECK_STATE2( receiveSocket, "Could not create ZMQ receive socket" );

        int val = CONSENSUS_ZMQ_HWM;
        auto rc = zmq_setsockopt( receiveSocket, ZMQ_RCVHWM, &val, sizeof( val ) );
        CHECK_STATE( rc == 0 );
        val = CONSENSUS_ZMQ_HWM;
        val = CONSENSUS_ZMQ_HWM;
        rc = zmq_setsockopt( receiveSocket, ZMQ_SNDHWM, &val, sizeof( val ) );
        CHECK_STATE( rc == 0 );

        CHECK_STATE( receiveSocket );

        CONS_LOG( debug, getThreadName() << " zmq debug: receiveSocket = "
                                    << to_string( ( uint64_t ) receiveSocket ) );

        int timeout = ZMQ_TIMEOUT;
        int linger = 0;

        zmq_setsockopt( receiveSocket, ZMQ_RCVTIMEO, &timeout, sizeof( int ) );
        zmq_setsockopt( receiveSocket, ZMQ_SNDTIMEO, &timeout, sizeof( int ) );
        zmq_setsockopt( receiveSocket, ZMQ_LINGER, &linger, sizeof( int ) );

        rc = zmq_bind( receiveSocket, ( "tcp://" + bindIP + ":" + to_string( bindPort ) ).c_str() );

        if ( rc != 0 ) {
            BOOST_THROW_EXCEPTION( FatalError(
                string( "Could not bind ZMQ server socket:" ) + zmq_strerror( errno ) ) );
        }

        CONS_LOG( debug, "Successfull bound ZMQ socket" );
    }
    return receiveSocket;
}


void ZMQSockets::closeReceive() {
    if ( closeReceiveCalled.exchange( true ) )
        return;

    LOCK( m );

    CONS_LOG( info, "consensus engine exiting: closing receive sockets" );

    if ( receiveSocket ) {
        if ( zmq_close( receiveSocket ) != 0 ) {
            CONS_LOG( err, "zmq_close returned an error on receiveSocket;" );
        }
        receiveSocket = nullptr;
    }
}


void ZMQSockets::closeSend() {
    if ( closeSendCalled.exchange( true ) )
        return;
    LOCK( m );
    CONS_LOG( info, "consensus engine exiting: closing ZMQ send sockets" );
    for ( auto&& item : sendSockets ) {
        if ( item.second ) {
            CONS_LOG( debug, getThreadName() << " zmq debug in closeSend(): closing "
                                        << to_string( ( uint64_t ) item.second ) );
            if ( zmq_close( item.second ) != 0 ) {
                CONS_LOG( err, "zmq_close returned an error on sendSocket;" );
            }
        }
    }
    sendSockets.clear();
}

void ZMQSockets::closeDestinationSocket( schain_index _schainIndex ) {
    LOCK( m );

    if ( sendSockets.count( _schainIndex ) == 0 ) {
        return;
    }

    auto socket = sendSockets.at( _schainIndex );
    sendSockets.erase( _schainIndex );

    if ( socket && zmq_close( socket ) != 0 ) {
        CONS_LOG( err, "zmq_close returned an error on peer sendSocket;" );
    }
}


void ZMQSockets::closeAndCleanupAll() {
    if ( closeAndCleanupAllCalled.exchange( true ) ) {
        return;
    }

    LOCK( m );

    CONS_LOG( info, "Cleaning up ZMQ sockets" );

    try {
        closeSend();
        closeReceive();
    } catch ( const exception& e ) {
        CONS_LOG( err, "Exception in zmq socket close:" << string( e.what() ) );
    }

    CONS_LOG( info, "Closing ZMQ context" );

    try {
        zmq_ctx_term( context );
    } catch ( const exception& ex ) {
        CONS_LOG( err, "Exception in zmq_ctx_term" );
        CONS_LOG( err, ex.what() );
    } catch ( ... ) {
        CONS_LOG( err, "Unknown exception in zmq_ctx_term" );
    }

    CONS_LOG( info, "Closed ZMQ context" );
}


ZMQSockets::~ZMQSockets() {}
