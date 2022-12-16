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

    @file BlockDB.h
    @author Stan Kladko
    @date 2019
*/


#ifndef SKALED_BLOCKDB_H
#define SKALED_BLOCKDB_H

class CommittedBlock;

#include "CacheLevelDB.h"

class CryptoManager;

class BlockDB : public CacheLevelDB {

    shared_mutex m;

    void saveBlock2LevelDB(const ptr<CommittedBlock> &_block);

    cache::lru_cache<uint64_t , ptr<vector<uint8_t>>> blockCache; // tsafe

public:

    BlockDB(Schain *_sChain, string &_dirname, string &_prefix, node_id _nodeId,
        uint64_t _maxDBSize);

    ptr<vector<uint8_t >> getSerializedBlockFromLevelDB(block_id _blockID);

    void saveBlock(const ptr<CommittedBlock> &_block);

    ptr<CommittedBlock> getBlock(block_id _blockID, const ptr<CryptoManager>& _cryptoManager);

    block_id readLastCommittedBlockID();

    const string& getFormatVersion() override;

    string createLastCommittedKey();

    string createBlockStartKey(block_id _blockID );

    bool unfinishedBlockExists( block_id _blockID );

    ptr<vector<uint8_t>> getSerializedBlocksFromLevelDB(block_id _startBlock, block_id _endBlock,
                                                        ptr<list<uint64_t>> _blockSizes);
};


#endif //SKALED_BLOCKDB_H
