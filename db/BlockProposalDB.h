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

    @file ReceivedBlockProposalsDatabase.h
    @author Stan Kladko
    @date 2018
*/

#pragma once


class BlockProposalSet;

class PartialHashesList;

class Schain;

class BooleanProposalVector;

#include "CacheLevelDB.h"
#include "thirdparty/lrucache.hpp"

class BlockProposalDB : public CacheLevelDB {

    recursive_mutex proposalCacheMutex;

    ptr<cache::lru_cache<string, ptr<BlockProposal>>> proposalCache;

public:


    bool proposalExists(block_id _blockId, schain_index _index);

    ptr<BlockProposal> getBlockProposal(block_id _blockID, schain_index _proposerIndex);

    BlockProposalDB(Schain *_sChain, string &_dirName, string &_prefix, node_id _nodeId, uint64_t _maxDBSize);

    void addBlockProposal(const ptr<BlockProposal>& _proposal);

    const string getFormatVersion();

    ptr<vector<uint8_t> > getSerializedProposalFromLevelDB(block_id _blockID, schain_index _proposerIndex);
};



