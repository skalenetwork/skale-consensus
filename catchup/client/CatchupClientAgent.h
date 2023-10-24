/*
    Copyright (C) 2018- SKALE Labs

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

    @file CatchupClientAgent.h
    @author Stan Kladko
    @date 2018 -
*/

#pragma once


class CommittedBlockList;
class ClientSocket;
class Schain;
class CatchupClientThreadPool;
class CatchupRequestHeader;
class CatchupResponseHeader;

class CatchupClientAgent : public Agent {
    ptr< CatchupClientThreadPool > catchupClientThreadPool = nullptr;

public:
    explicit CatchupClientAgent( Schain& _sChain );


    [[nodiscard]] uint64_t sync( schain_index _dstIndex );


    static void workerThreadItemSendLoop( CatchupClientAgent* _agent );

    nlohmann::json readCatchupResponseHeader(
        const ptr< ClientSocket >& _socket, ptr< CatchupRequestHeader > _requestHeader );


    ptr< CommittedBlockList > readMissingBlocks( ptr< ClientSocket >& _socket,
        nlohmann::json& _responseHeader, ptr< CatchupRequestHeader > _requestHeader );


    size_t parseBlockSizes( nlohmann::json _responseHeader,
        const ptr< vector< uint64_t > >& _blockSizes, ptr< CatchupRequestHeader > _requestHeader );

    static schain_index nextSyncNodeIndex(
        const CatchupClientAgent* _agent, schain_index _destinationSchainIndex );
};
