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

    @file AbstractServerAgent.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"
#include "Log.h"
#include <exceptions/PingException.h>
#include "chains/Schain.h"
#include "Agent.h"

#include "exceptions/OldBlockIDException.h"
#include "exceptions/ExitRequestedException.h"


#include "blockproposal/pusher/BlockProposalClientAgent.h"
#include "db/BlockProposalDB.h"
#include "headers/MissingTransactionsRequestHeader.h"
#include "network/Buffer.h"
#include "network/IO.h"
#include "network/Network.h"
#include "network/ServerConnection.h"
#include "network/Sockets.h"
#include "network/TCPServerSocket.h"
#include "threads/GlobalThreadRegistry.h"
#include "utils/LogThrottle.h"
#include <chrono>


#include "AbstractServerAgent.h"

bool AbstractServerAgent::tryPushToQueueAndNotifyWorker(
    const ptr< ServerConnection >& _connectionEnvelope ) {
    return pendingConnections.tryPush( _connectionEnvelope );
}

ptr< ServerConnection > AbstractServerAgent::workerThreadWaitandPopConnection() {
    if ( getSchain()->getNode()->isExitRequested() )
        return nullptr;

    auto connection = pendingConnections.waitAndPop();
    if ( getSchain()->getNode()->isExitRequested() )
        return nullptr;
    return connection;
}


void AbstractServerAgent::workerThreadConnectionProcessingLoop( void* _params ) {
    CHECK_ARGUMENT( _params );
    AbstractServerAgent* server = ( reinterpret_cast< AbstractServerAgent* >( _params ) );
    CHECK_STATE( server );

    server->waitOnGlobalStartBarrier();

    logThreadLocal_ = server->getSchain()->getNode()->getLog();

    CONS_LOG( trace, "Started server loop" );

    while ( !server->getNode()->isExitRequested() ) {
        ptr< ServerConnection > connection = nullptr;
        try {
            connection = server->workerThreadWaitandPopConnection();
            if ( server->getNode()->isExitRequested() )
                return;  // notice - connection is nullptr in this case
            CHECK_STATE( connection );
            server->processNextAvailableConnection( connection );
        } catch ( PingException& e ) {
            CONS_LOG( info, e.what() );
        } catch ( exception& e ) {
            SkaleException::logNested( e );
        }
    }
}


void AbstractServerAgent::send(
    const ptr< ServerConnection >& _connectionEnvelope, const ptr< Header >& _header ) {
    CHECK_ARGUMENT( _connectionEnvelope );
    CHECK_ARGUMENT( _header );
    CHECK_ARGUMENT( _header->isComplete() );

    auto buf = _header->toBuffer();
    getSchain()->getIo()->writeBuf( _connectionEnvelope->getDescriptor(), buf );
}

AbstractServerAgent::AbstractServerAgent(
    const string& _name, Schain& _schain, const ptr< TCPServerSocket >& _socket )
    : Agent( _schain, true ), name( _name ), socket( _socket ), networkReadThread( nullptr ) {
}

AbstractServerAgent::~AbstractServerAgent() {
    if ( networkReadThread && networkReadThread->joinable() )
        networkReadThread->join();
}

void AbstractServerAgent::acceptTCPConnectionsLoop() {
    try {
        logThreadLocal_ = getSchain()->getNode()->getLog();
        setThreadName( name, getSchain()->getNode()->getConsensusEngine() );
        waitOnGlobalStartBarrier();

        const auto tcpSocket = dynamic_pointer_cast< TCPServerSocket >( socket );
        CHECK_STATE( tcpSocket );

        LogThrottle resourceWarnings( std::chrono::seconds( 5 ) );
        LogThrottle admissionWarnings( std::chrono::seconds( 5 ) );
        bool recoveringFromResourceError = false;

        while ( !getSchain()->getNode()->isExitRequested() ) {
            auto result = tcpSocket->acceptConnection();
            if ( getNode()->isExitRequested() )
                return;

            if ( result.status != TCPServerSocket::AcceptStatus::Accepted ) {
                if ( result.status == TCPServerSocket::AcceptStatus::RetryableError )
                    continue;
                if ( result.status == TCPServerSocket::AcceptStatus::ResourceExhausted ) {
                    if ( resourceWarnings.shouldLog() ) {
                        CONS_LOG( warn, name << ": accept resource exhaustion; retrying"
                            << "; events since last report: " << resourceWarnings.lastReportEventCount()
                            << "; errno: " << result.error );
                    }
                    recoveringFromResourceError = true;
                    std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
                    continue;
                }

                BOOST_THROW_EXCEPTION( FatalError(
                    "accept failed:" + string( strerror( result.error ) ), __CLASS_NAME__ ) );
            }

            if ( recoveringFromResourceError ) {
                CONS_LOG( info, name << ": TCP acceptance recovered from resource exhaustion" );
                recoveringFromResourceError = false;
            }

            if ( !tryPushToQueueAndNotifyWorker( result.connection ) ) {
                if ( admissionWarnings.shouldLog() ) {
                    CONS_LOG( warn, name << ": TCP pending queue full; connection rejected"
                        << "; events since last report: " << admissionWarnings.lastReportEventCount() );
                }
            }
        }
    } catch ( ExitRequestedException& e ) {
        if ( !getNode()->isExitRequested() ) {
            SkaleException::logNested( e );
            getNode()->initiateApplicationExitOnFatalConsensusError( e.what() );
        }
    } catch ( std::exception& e ) {
        SkaleException::logNested( e );
        getNode()->initiateApplicationExitOnFatalConsensusError( e.what() );
    } catch ( ... ) {
        CONS_LOG( critical, name << ": unknown exception in TCP accept thread" );
        getNode()->initiateApplicationExitOnFatalConsensusError(
            "Unknown exception in TCP accept thread" );
    }
}

void AbstractServerAgent::createNetworkReadThread() {
    CONS_LOG( trace, name << " Starting TCP server network read loop" );
    networkReadThread =
        make_shared< thread >( std::bind( &AbstractServerAgent::acceptTCPConnectionsLoop, this ) );
    getThreadRegistry()->add( networkReadThread );
    CONS_LOG( trace, name << " Started TCP server network read loop" );
}


void AbstractServerAgent::notifyAllConditionVariables() {
    Agent::notifyAllConditionVariables();
    CONS_LOG( trace, name << ": stopping TCP pending queue" );
    pendingConnections.shutdown();
}
