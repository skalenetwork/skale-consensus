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

    @file ZMQNetwork.cpp
    @author Stan Kladko
    @date 2018
*/

#include "Log.h"
#include "SkaleCommon.h"
#include "exceptions/FatalError.h"


#include "Network.h"

#include "zmq.h"

#include "blockproposal/pusher/BlockProposalClientAgent.h"
#include "db/BlockProposalDB.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/IOException.h"
#include "exceptions/NetworkProtocolException.h"
#include "messages/NetworkMessage.h"
#include "messages/NetworkMessageEnvelope.h"
#include "node/NodeInfo.h"
#include "pendingqueue/PendingTransactionsAgent.h"

#include "Buffer.h"
#include "Sockets.h"
#include "ZMQNetwork.h"
#include "chains/Schain.h"

#include "ZMQSockets.h"

using namespace std;


bool ZMQNetwork::sendMessage(
    const ptr< NodeInfo >& _remoteNodeInfo, const ptr< NetworkMessage >& _msg ) {
    CHECK_ARGUMENT( _remoteNodeInfo );
    CHECK_ARGUMENT( _msg );

    auto buf = _msg->serializeToString();

    void* s = sChain->getNode()->getSockets()->consensusZMQSockets->getDestinationSocket(
        _remoteNodeInfo );


    return interruptableSend( s, buf.data(), buf.size() );
}


uint64_t ZMQNetwork::interruptableRecv( void* _socket, void* _buf, size_t _len ) {


    int rc;


    zmq_pollitem_t items[1];
    items[0].socket = _socket;
    items[0].events = ZMQ_POLLIN;


    for ( ;; ) {

        int pollResult = 0;

        do {
            pollResult = zmq_poll(items, 1, 1000);
            if (this->getNode()->isExitRequested()) {
                BOOST_THROW_EXCEPTION( ExitRequestedException( __CLASS_NAME__ ) );
            }
        } while (pollResult == 0);

        rc = zmq_recv( _socket, _buf, _len, 0 );

        if ( this->getNode()->isExitRequested() ) {
            LOG( debug,
                getThreadName() + " zmq debug: closing = " + to_string( ( uint64_t ) _socket ) );
            BOOST_THROW_EXCEPTION( ExitRequestedException( __CLASS_NAME__ ) );
        }

        if ( rc >= 0 )
            return ( uint64_t ) rc;

        if ( errno != EAGAIN ) {
            BOOST_THROW_EXCEPTION( NetworkProtocolException(
                "Zmq recv failed " + string( zmq_strerror( errno ) ), __CLASS_NAME__ ) );
        }
    }
}


bool ZMQNetwork::interruptableSend( void* _socket, void* _buf, size_t _len ) {
    auto simulatedDelay = sChain->getNode()->getSimulateNetworkWriteDelayMs();

    if ( simulatedDelay > 0 )
        usleep( 1000 * sChain->getNode()->getSimulateNetworkWriteDelayMs() );

    int rc;


    int flags = ZMQ_DONTWAIT;

    rc = zmq_send( _socket, _buf, _len, flags );

    if ( this->getNode()->isExitRequested() ) {
        LOG( debug, getThreadName() + "zmq debug: closing = " + to_string( ( uint64_t ) _socket ) );
        BOOST_THROW_EXCEPTION( ExitRequestedException( __CLASS_NAME__ ) );
    }

    if ( rc < 0 && errno == EAGAIN ) {
        return false;
    }

    if ( rc < 0 ) {
        BOOST_THROW_EXCEPTION( NetworkProtocolException(
            "Zmq send failed " + string( zmq_strerror( errno ) ), __CLASS_NAME__ ) );
    }

    return true;
}

uint64_t ZMQNetwork::readMessageFromNetwork( const ptr< Buffer > buf ) {
    auto s = sChain->getNode()->getSockets()->consensusZMQSockets->getReceiveSocket();

    auto rc = interruptableRecv( s, buf->getBuf()->data(), MAX_CONSENSUS_MESSAGE_LEN );

    if ( ( uint64_t ) rc >= MAX_CONSENSUS_MESSAGE_LEN ) {
        BOOST_THROW_EXCEPTION( NetworkProtocolException(
            "Consensus Message length too large:" + to_string( rc ), __CLASS_NAME__ ) );
    }

    return rc;
}



ZMQNetwork::ZMQNetwork( Schain& _schain ) : Network( _schain ) {}