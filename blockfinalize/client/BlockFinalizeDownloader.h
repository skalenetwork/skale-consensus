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

    @file BlockFinalizeDownloader.h
    @author Stan Kladko
    @date 2019
*/

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

    @file BlockFinalizeDownloader.h
    @author Stan Kladko
    @date 2018
*/

#pragma once



class CommittedBlockList;
class ClientSocket;
class Schain;
class BlockFinalizeResponseHeader;
class BlockProposalFragment;
class BlockProposalFragmentList;
class BlockProposal;
class BlockFinalizeDownloaderThreadPool;
class BlockProposalSet;

#include "thirdparty/rapidjson/document.h"
#include "thirdparty/rapidjson/prettywriter.h"
#include "datastructures/BlockProposalFragmentList.h"

class BlockFinalizeDownloader : public Agent {

    block_id blockId;

    schain_index proposerIndex;

    BlockProposalFragmentList fragmentList;

public:

    atomic< uint64_t > threadCounter;

    ptr<BlockFinalizeDownloaderThreadPool> threadPool = nullptr;

    BlockFinalizeDownloader(Schain *_sChain, block_id _blockId, schain_index _proposerIndex);


    virtual ~BlockFinalizeDownloader();

    uint64_t downloadFragment(schain_index _dstIndex, fragment_index _fragmentIndex);


    static void workerThreadFragmentDownloadLoop(BlockFinalizeDownloader* _agent, schain_index _dstIndex );

    rapidjson::Document readBlockFinalizeResponseHeader( const ptr< ClientSocket >& _socket );


    ptr<BlockProposalFragment>
    readBlockFragment(const ptr<ClientSocket>& _socket, rapidjson::Document& responseHeader, fragment_index _fragmentIndex,
                      node_count _nodeCount);

    uint64_t readFragmentSize(rapidjson::Document& _responseHeader);

    ptr<BlockProposal> downloadProposal();

    uint64_t readBlockSize(rapidjson::Document& _responseHeader);

    string readBlockHash(rapidjson::Document& _responseHeader);

    block_id getBlockId();

    schain_index getProposerIndex();
};

