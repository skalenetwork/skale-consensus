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
class CommittedBlockFragment;
class CommittedBlockFragmentList;

class BlockFinalizeDownloaderThreadPool;
class BlockProposalSet;

#include "../../datastructures/CommittedBlockFragmentList.h"

class BlockFinalizeDownloader {

    Schain* sChain;
public:
    Schain *getSchain() const;

private:

    block_id blockId;
public:
    virtual ~BlockFinalizeDownloader();

private:
    schain_index proposerIndex;

    CommittedBlockFragmentList fragmentList;

    ptr<BlockProposalSet> proposalSet;

public:
    atomic< uint64_t > threadCounter;

    BlockFinalizeDownloaderThreadPool* threadPool = nullptr;

    BlockFinalizeDownloader(Schain* _sChain, block_id _blockId, schain_index _proposerIndex,
                            ptr<BlockProposalSet> proposalSet);


    uint64_t downloadFragment(schain_index _dstIndex, fragment_index _fragmentIndex);


    static void workerThreadFragmentDownloadLoop(BlockFinalizeDownloader* agent, schain_index _dstIndex );

    nlohmann::json readBlockFinalizeResponseHeader( ptr< ClientSocket > _socket );


    ptr<CommittedBlockFragment>
    readBlockFragment(ptr<ClientSocket> _socket, nlohmann::json responseHeader, fragment_index _fragmentIndex,
                      node_count _nodeCount);

    uint64_t readFragmentSize(nlohmann::json _responseHeader);

    ptr<CommittedBlock>  downloadProposal();

    uint64_t readBlockSize(nlohmann::json _responseHeader);

    ptr<string> readBlockHash(nlohmann::json _responseHeader);
};

