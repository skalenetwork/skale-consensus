/*
    Copyright (C) 2022- SKALE Labs

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

    @file BlockDecryptDownloader.h
    @author Stan Kladko
    @date 2021
*/

/*
    Copyright (C) 2022- SKALE Labs

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

    @file BlockDecryptDownloader.h
    @author Stan Kladko
    @date 2022-
*/

#pragma once



class CommittedBlockList;
class ClientSocket;
class Schain;
class BlockDecryptResponseHeader;
class BlockProposalFragment;
class BlockProposalFragmentList;
class BlockProposal;
class BlockDecryptDownloaderThreadPool;
class BlockProposalSet;

#include "datastructures/BlockProposalFragmentList.h"

class BlockDecryptDownloader : public Agent {

    block_id blockId = 0;

    schain_index proposerIndex = 0;

    BlockProposalFragmentList fragmentList;

public:

    ptr<BlockDecryptDownloaderThreadPool> threadPool = nullptr;

    BlockDecryptDownloader(Schain *_sChain, block_id _blockId, schain_index _proposerIndex);


    ~BlockDecryptDownloader() override;

    uint64_t downloadFragment(schain_index _dstIndex, fragment_index _fragmentIndex);


    static void workerThreadDecryptionDownloadLoop(BlockDecryptDownloader* _agent, schain_index _dstIndex );

    nlohmann::json readBlockDecryptResponseHeader( const ptr< ClientSocket >& _socket );


    ptr<BlockProposalFragment>
    readBlockFragment(const ptr<ClientSocket>& _socket, nlohmann::json responseHeader, fragment_index _fragmentIndex,
                      node_count _nodeCount);

    static uint64_t readFragmentSize(nlohmann::json _responseHeader);

    ptr<BlockProposal> downloadProposal();

    uint64_t readBlockSize(nlohmann::json _responseHeader);

    string readBlockHash(nlohmann::json _responseHeader);

    block_id getBlockId();

    schain_index getProposerIndex();
};

