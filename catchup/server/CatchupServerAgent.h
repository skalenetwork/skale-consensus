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

    @file CatchupServerAgent.h
    @author Stan Kladko
    @date 2018
*/

#pragma once

#include "abstracttcpserver/AbstractServerAgent.h"
#include "abstracttcpserver/ConnectionStatus.h"
#include <condition_variable>
#include <mutex>
#include <queue>

#include "datastructures/PartialHashesList.h"
#include "headers/Header.h"
#include "network/ServerConnection.h"

#include "Agent.h"
#include "CatchupWorkerThreadPool.h"

class CommittedBlock;
class CommittedBlockList;
class CatchupResponseHeader;
class BlockFinalizeResponseHeader;

class CatchupServerAgent : public AbstractServerAgent {
    ptr< CatchupWorkerThreadPool > catchupWorkerThreadPool;

    ptr< vector< uint8_t > > createBlockCatchupResponse( nlohmann::json _jsonRequest,
        const ptr< CatchupResponseHeader >& _responseHeader, block_id _blockID );


    ptr< vector< uint8_t > > createBlockFinalizeResponse( nlohmann::json _jsonRequest,
        const ptr< BlockFinalizeResponseHeader >& _responseHeader, block_id _blockID );


public:
    CatchupServerAgent( Schain& _schain, const ptr< TCPServerSocket >& _s );

    ~CatchupServerAgent() override;

    ptr< vector< uint8_t > > createResponseHeaderAndBinary(
        const ptr< ServerConnection >& _connectionEnvelope, nlohmann::json _jsonRequest,
        const ptr< Header >& _responseHeader );

    void processNextAvailableConnection( const ptr< ServerConnection >& _connection ) override;
};
