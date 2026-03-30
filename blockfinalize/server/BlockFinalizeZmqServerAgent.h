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

#pragma once

#include "Agent.h"

class BlockFinalizeResponder;
class BlockFinalizeZmqServerThreadPool;
class ZMQSockets;

/**
 * ZMQ server for BlockFinalize requests on the bulk-data lane.
 *
 * This is the new persistent request/reply transport used by upgraded nodes. It intentionally does
 * not own BlockFinalize semantics itself; it only decodes the ZMQ frame, delegates to
 * BlockFinalizeResponder, then sends the response frame back.
 */
class BlockFinalizeZmqServerAgent : public Agent {
    ptr< ZMQSockets > sockets;
    ptr< BlockFinalizeResponder > blockFinalizeResponder;
    ptr< BlockFinalizeZmqServerThreadPool > workerThreadPool;

    // Handle one ZMQ BlockFinalize request on the already-bound receive socket.
    void processNextRequest( void* _socket );

public:
    BlockFinalizeZmqServerAgent( Schain& _schain, const ptr< ZMQSockets >& _sockets );
    ~BlockFinalizeZmqServerAgent() override;

    // Main receive loop for the bulk-data ZMQ lane.
    static void workerThreadZmqServerLoop( BlockFinalizeZmqServerAgent* _agent );
};
