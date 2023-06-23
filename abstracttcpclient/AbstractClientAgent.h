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

    @file AbstractClientAgent.h
    @author Stan Kladko
    @date 2019
*/

#ifndef CONSENSUS_ABSTRACTCLIENTAGENT_H
#define CONSENSUS_ABSTRACTCLIENTAGENT_H


#include "Agent.h"
#include "abstracttcpclient/AbstractClientAgent.h"
#include <abstracttcpserver/ConnectionStatus.h>
#include <exceptions/ConnectionRefusedException.h>

#include "datastructures/SendableItem.h"

class DataStructure;
class BlockProposal;
class DAProof;
class ClientSocket;

class AbstractClientAgent : public Agent {
protected:
    port_type portType;

    atomic< uint64_t > threadCounter;

    explicit AbstractClientAgent( Schain& _sChain, port_type _portType );

protected:
    void sendItem( const ptr< SendableItem >& _item, schain_index _dstIndex );

    virtual pair< ConnectionStatus, ConnectionSubStatus > sendItemImpl(
        const ptr< SendableItem >& _item, const ptr< ClientSocket >& _socket,
        schain_index _destIndex ) = 0;

    std::map< schain_index, ptr< queue< ptr< SendableItem > > > > itemQueue;  // thread safe

    uint64_t incrementAndReturnThreadCounter();

    void enqueueItemImpl( const ptr< SendableItem >& _item );

public:
    static void workerThreadItemSendLoop( AbstractClientAgent* agent );

    void enqueueItem( const ptr< BlockProposal >& _item );

    void enqueueItem( const ptr< DAProof >& _item );
};


bool operator==( const ptr< partial_sha_hash >& a, const ptr< partial_sha_hash >& b );

#endif  // CONSENSUS_ABSTRACTCLIENTAGENT_H