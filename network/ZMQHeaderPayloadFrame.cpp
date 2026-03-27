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
*/

#include "SkaleCommon.h"

#include "chains/Schain.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/NetworkProtocolException.h"
#include "exceptions/ParsingException.h"
#include "exceptions/ZMQTransportException.h"
#include "headers/Header.h"
#include "network/ZMQErrorClassifier.h"
#include "node/Node.h"
#include "utils/Time.h"
#include "ZMQHeaderPayloadFrame.h"
#include "zmq.h"

using namespace std;

namespace {
void maybeSimulateDelay( Schain& _sChain ) {
    auto simulatedDelay = _sChain.getNode()->getSimulateNetworkWriteDelayMs();
    if ( simulatedDelay > 0 ) {
        usleep( simulatedDelay * 1000 );
    }
}

[[noreturn]] void throwZMQCommunicationException( const string& _message ) {
    auto zmqErrno = zmq_errno();
    auto errorMessage = _message + ":" + string( zmq_strerror( zmqErrno ) );

    if ( isZMQTransportErrorForTcpFallback( zmqErrno ) ) {
        BOOST_THROW_EXCEPTION( ZMQTransportException( errorMessage, zmqErrno, __CLASS_NAME__ ) );
    }

    BOOST_THROW_EXCEPTION( NetworkProtocolException( errorMessage, __CLASS_NAME__ ) );
}
}

ptr< vector< uint8_t > > ZMQHeaderPayloadFrame::packMessage(
    const ptr< Header >& _header, const ptr< vector< uint8_t > >& _payload ) {
    CHECK_ARGUMENT( _header );
    CHECK_ARGUMENT( _header->isComplete() );

    auto headerStr = _header->serializeToString();
    auto headerLen = ( uint64_t ) headerStr.size();
    auto payloadLen = _payload ? _payload->size() : 0;
    auto frame = make_shared< vector< uint8_t > >( sizeof( headerLen ) + headerLen + payloadLen );

    memcpy( frame->data(), &headerLen, sizeof( headerLen ) );
    memcpy( frame->data() + sizeof( headerLen ), headerStr.data(), headerLen );

    if ( _payload && !_payload->empty() ) {
        memcpy( frame->data() + sizeof( headerLen ) + headerLen, _payload->data(), payloadLen );
    }

    return frame;
}

nlohmann::json ZMQHeaderPayloadFrame::unpackMessage( const void* _data, size_t _len,
    ptr< vector< uint8_t > >& _payload, const char* _errorString, uint64_t _maxHeaderLen ) {
    CHECK_ARGUMENT( _data );
    CHECK_ARGUMENT( _errorString );

    if ( _len < sizeof( uint64_t ) ) {
        BOOST_THROW_EXCEPTION( ParsingException(
            string( _errorString ) + ":frame too short", __CLASS_NAME__ ) );
    }

    uint64_t headerLen = 0;
    memcpy( &headerLen, _data, sizeof( headerLen ) );

    if ( headerLen < 2 || headerLen > _maxHeaderLen || sizeof( headerLen ) + headerLen > _len ) {
        BOOST_THROW_EXCEPTION( ParsingException(
            string( _errorString ) + ":invalid header len:" + to_string( headerLen ),
            __CLASS_NAME__ ) );
    }

    auto headerData = reinterpret_cast< const char* >( _data ) + sizeof( headerLen );
    auto header = string( headerData, headerLen );

    try {
        auto js = nlohmann::json::parse( header );
        auto payloadLen = _len - sizeof( headerLen ) - headerLen;

        if ( payloadLen > 0 ) {
            _payload = make_shared< vector< uint8_t > >( payloadLen );
            memcpy( _payload->data(), headerData + headerLen, payloadLen );
        } else {
            _payload = nullptr;
        }

        return js;
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        BOOST_THROW_EXCEPTION( ParsingException(
            string( _errorString ) + ":could not parse frame header", __CLASS_NAME__ ) );
    }
}

void ZMQHeaderPayloadFrame::sendFrame( Schain& _sChain, void* _socket, const ptr< vector< uint8_t > >& _frame ) {
    CHECK_ARGUMENT( _socket );
    CHECK_ARGUMENT( _frame );

    maybeSimulateDelay( _sChain );

    auto rc = zmq_send( _socket, _frame->data(), _frame->size(), 0 );
    if ( rc < 0 ) {
        throwZMQCommunicationException( "ZMQ send failed" );
    }
}

void ZMQHeaderPayloadFrame::sendFrameToRoutingId( Schain& _sChain, void* _socket, uint32_t _routingId,
    const ptr< vector< uint8_t > >& _frame ) {
    CHECK_ARGUMENT( _socket );
    CHECK_ARGUMENT( _frame );
    CHECK_ARGUMENT( _routingId > 0 );

    maybeSimulateDelay( _sChain );

    zmq_msg_t msg;
    auto rc = zmq_msg_init_size( &msg, _frame->size() );
    CHECK_STATE( rc == 0 );

    memcpy( zmq_msg_data( &msg ), _frame->data(), _frame->size() );
    rc = zmq_msg_set_routing_id( &msg, _routingId );
    CHECK_STATE( rc == 0 );

    rc = zmq_msg_send( &msg, _socket, 0 );
    if ( rc < 0 ) {
        zmq_msg_close( &msg );
        throwZMQCommunicationException( "ZMQ send failed" );
    }

    rc = zmq_msg_close( &msg );
    CHECK_STATE( rc == 0 );
}

ptr< vector< uint8_t > > ZMQHeaderPayloadFrame::receiveFrame(
    Schain& _sChain, void* _socket, uint32_t* _routingId, const char* _errorString,
    bool _throwOnTimeout ) {
    CHECK_ARGUMENT( _socket );
    CHECK_ARGUMENT( _errorString );

    zmq_msg_t msg;
    auto rc = zmq_msg_init( &msg );
    CHECK_STATE( rc == 0 );

    rc = zmq_msg_recv( &msg, _socket, 0 );
    if ( rc < 0 ) {
        auto zmqErrno = zmq_errno();
        zmq_msg_close( &msg );
        if ( zmqErrno == EAGAIN ) {
            // check if timeout happened during exit
            _sChain.getNode()->exitCheck();

            // If we're here, it means we got a timeout but exit was not requested. 
            if ( !_throwOnTimeout ) {
                return nullptr;
            }
        }
        auto errorMessage = string( _errorString ) + ":" + string( zmq_strerror( zmqErrno ) );
        if ( isZMQTransportErrorForTcpFallback( zmqErrno ) ) {
            BOOST_THROW_EXCEPTION(
                ZMQTransportException( errorMessage, zmqErrno, __CLASS_NAME__ ) );
        }
        BOOST_THROW_EXCEPTION( NetworkProtocolException( errorMessage, __CLASS_NAME__ ) );
    }

    auto frame = make_shared< vector< uint8_t > >( rc );
    memcpy( frame->data(), zmq_msg_data( &msg ), rc );

    if ( _routingId ) {
        *_routingId = zmq_msg_routing_id( &msg );
    }

    zmq_msg_close( &msg );
    return frame;
}
