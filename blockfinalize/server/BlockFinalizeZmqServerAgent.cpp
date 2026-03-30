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

#include "blockfinalize/server/BlockFinalizeResponder.h"
#include "blockfinalize/server/BlockFinalizeZmqServerAgent.h"
#include "blockfinalize/server/BlockFinalizeZmqServerThreadPool.h"

#include "chains/Schain.h"
#include "exceptions/CouldNotSendMessageException.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/InvalidMessageFormatException.h"
#include "exceptions/NetworkProtocolException.h"
#include "exceptions/SkaleException.h"
#include "headers/BlockFinalizeResponseHeader.h"
#include "headers/Header.h"
#include "monitoring/LivelinessMonitor.h"
#include "network/Sockets.h"
#include "network/ZMQHeaderPayloadFrame.h"
#include "network/ZMQSockets.h"
#include "node/Node.h"

BlockFinalizeZmqServerAgent::BlockFinalizeZmqServerAgent(
    Schain& _schain, const ptr< ZMQSockets >& _sockets )
    : Agent( _schain, true ), sockets( _sockets ) {
    CHECK_ARGUMENT( _sockets );

    blockFinalizeResponder = make_shared< BlockFinalizeResponder >( _schain );
    // ZMQ receive sockets are used from a single reader thread in the existing consensus path.
    // Keep the bulk-data receive side on the same model and scale later via a different socket layout.
    workerThreadPool = make_shared< BlockFinalizeZmqServerThreadPool >( num_threads( 1 ), this );
    workerThreadPool->startService();
}

BlockFinalizeZmqServerAgent::~BlockFinalizeZmqServerAgent() {
    if ( workerThreadPool ) {
        workerThreadPool->joinAll();
    }
}

void BlockFinalizeZmqServerAgent::workerThreadZmqServerLoop( BlockFinalizeZmqServerAgent* _agent ) {
    CHECK_ARGUMENT( _agent );

    logThreadLocal_ = _agent->getNode()->getLog();
    _agent->waitOnGlobalStartBarrier();

    auto socket = _agent->sockets->getReceiveSocket();

    while ( !_agent->getNode()->isExitRequested() ) {
        try {
            _agent->processNextRequest( socket );
        } catch ( ExitRequestedException& ) {
            break;
        } catch ( exception& e ) {
            SkaleException::logNested( e );
        }
    }
}

void BlockFinalizeZmqServerAgent::processNextRequest( void* _socket ) {
    CHECK_ARGUMENT( _socket );

    uint32_t routingId = 0;
    auto requestFrame = ZMQHeaderPayloadFrame::receiveFrame(
        *getSchain(), _socket, &routingId, "Could not read ZMQ request", false );
    if ( !requestFrame ) {
        return;
    }
    CHECK_STATE( routingId > 0 );

    ptr< vector< uint8_t > > requestPayload;
    auto jsonRequest = ZMQHeaderPayloadFrame::unpackMessage(
        requestFrame->data(), requestFrame->size(), requestPayload, "Could not parse ZMQ request" );
    CHECK_STATE( !requestPayload );

    auto responseHeader = make_shared< BlockFinalizeResponseHeader >();
    ptr< vector< uint8_t > > serializedBinary = nullptr;

    try {
        auto type = Header::getString( jsonRequest, "type" );
        if ( type.compare( Header::BLOCK_FINALIZE_REQ ) != 0 ) {
            BOOST_THROW_EXCEPTION( InvalidMessageFormatException(
                "Unknown bulk-data request type:" + type, __CLASS_NAME__ ) );
        }

        getSchain()->noteBlockFinalizeZmqServerRequestServed();
        auto request = BlockFinalizeResponder::parseRequest( jsonRequest );
        MONITOR( "BlockFinalizeResponder", __FUNCTION__ );
        serializedBinary = blockFinalizeResponder->createResponse( request, responseHeader );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        try {
            responseHeader->setStatusSubStatus( CONNECTION_ERROR, CONNECTION_SUBSTATUS_UNKNOWN );
            responseHeader->setComplete();
        } catch ( ... ) {
        }
    }

    try {
        auto responseFrame = ZMQHeaderPayloadFrame::packMessage( responseHeader, serializedBinary );
        ZMQHeaderPayloadFrame::sendFrameToRoutingId( *getSchain(), _socket, routingId, responseFrame );
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested(
            CouldNotSendMessageException( "Could not send ZMQ BlockFinalize response", __CLASS_NAME__ ) );
    }
}
