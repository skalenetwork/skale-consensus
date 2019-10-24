/*
    Copyright (C) 2019 SKALE Labs

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

    @file AbstractClientAgent.cpp
    @author Stan Kladko
    @date 2019
*/

#include "../../Log.h"
#include "../SkaleCommon.h"
#include "../thirdparty/json.hpp"

#include "../../Agent.h"
#include "../../abstracttcpclient/AbstractClientAgent.h"
#include "../../node/Node.h"
#include "../exceptions/Exception.h"
#include "../exceptions/ExitRequestedException.h"
#include "../exceptions/FatalError.h"
#include "../exceptions/NetworkProtocolException.h"
#include "../network/ClientSocket.h"
#include "../network/IO.h"

#include "../SkaleCommon.h"
#include "../chains/Schain.h"
#include "../crypto/SHAHash.h"
#include "../node/Node.h"
#include "../node/NodeInfo.h"
#include "../thirdparty/json.hpp"


#include "../datastructures/BlockProposal.h"
#include "../datastructures/DAProof.h"

#include "../thirdparty/json.hpp"

#include "AbstractClientAgent.h"


AbstractClientAgent::AbstractClientAgent( Schain& _sChain, port_type _portType )
    : Agent( _sChain, false ) {
    portType = _portType;


    logThreadLocal_ = _sChain.getNode()->getLog();

    for ( uint64_t i = 1; i <= _sChain.getNodeCount(); i++ ) {
        ( itemQueue ).emplace( schain_index( i ), make_shared< queue< ptr< DataStructure > > >() );
        ( queueCond ).emplace( schain_index( i ), make_shared< condition_variable >() );
        ( queueMutex ).emplace( schain_index( i ), make_shared< std::mutex >() );

        ASSERT( itemQueue.count( schain_index( i ) ) );
        ASSERT( queueCond.count( schain_index( i ) ) );
        ASSERT( queueMutex.count( schain_index( i ) ) );
    }

    threadCounter = 0;
}

uint64_t AbstractClientAgent::incrementAndReturnThreadCounter() {
    return threadCounter++;  // l_sergiy: increment/return-prev of atomic variable
}


void AbstractClientAgent::sendItem(
        ptr<DataStructure> _item, schain_index _dstIndex, node_id _dstNodeId ) {
    ASSERT( getNode()->isStarted() );

    auto socket = make_shared< ClientSocket >( *sChain, _dstIndex, portType );


    try {
        getSchain()->getIo()->writeMagic( socket );
    }

    catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( NetworkProtocolException( "Could not write magic", __CLASS_NAME__ ) );
    }


    sendItemImpl(_item, socket, _dstIndex, _dstNodeId );
}


void AbstractClientAgent::enqueueItemImpl(ptr<DataStructure> item ) {
    for ( uint64_t i = 1; i <= ( uint64_t ) this->sChain->getNodeCount(); i++ ) {
        {
            std::lock_guard< std::mutex > lock( *queueMutex[schain_index( i )] );
            auto q = itemQueue[schain_index( i )];
            q->push( item );
        }
        queueCond.at( schain_index( i ) )->notify_all();
    }
}




void AbstractClientAgent::workerThreadItemSendLoop( AbstractClientAgent* agent ) {
    agent->waitOnGlobalStartBarrier();

    auto destinationSchainIndex = schain_index( agent->incrementAndReturnThreadCounter() + 1 );

    try {
        while ( !agent->getSchain()->getNode()->isExitRequested() ) {
            {
                std::unique_lock< std::mutex > mlock( *agent->queueMutex[destinationSchainIndex] );


                while ( agent->itemQueue[destinationSchainIndex]->empty() ) {
                    agent->getSchain()->getNode()->exitCheck();
                    agent->queueCond[destinationSchainIndex]->wait( mlock );
                }
            }

            ASSERT( agent->itemQueue[destinationSchainIndex] );

            auto proposal = agent->itemQueue[destinationSchainIndex]->front();


            ASSERT( proposal );

            agent->itemQueue[destinationSchainIndex]->pop();


            if ( destinationSchainIndex != ( agent->getSchain()->getSchainIndex() ) ) {
                auto nodeId = agent->getSchain()
                                  ->getNode()
                                  ->getNodeInfoByIndex( destinationSchainIndex )
                                  ->getNodeID();

                bool sent = false;

                while ( !sent ) {
                    try {
                        agent->sendItem( proposal, destinationSchainIndex, nodeId );
                        sent = true;
                    } catch ( Exception& e ) {
                        Exception::logNested( e );
                        if ( agent->getNode()->isExitRequested() )
                            return;
                        usleep( agent->getNode()->getWaitAfterNetworkErrorMs() * 1000 );
                    }
                }
            }
        };
    }

    catch ( FatalError* e ) {
        agent->getNode()->exitOnFatalError( e->getMessage() );
    } catch ( ExitRequestedException& e ) {
        return;
    }
}

void AbstractClientAgent::enqueueItem(ptr<BlockProposal> _item) {
    enqueueItemImpl(_item);
}

void AbstractClientAgent::enqueueItem(ptr<DAProof> _item) {
    enqueueItemImpl(_item);
}
