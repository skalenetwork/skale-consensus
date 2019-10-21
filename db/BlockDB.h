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

#include "LevelDB.h"

class CryptoManager;

class BlockDB : public LevelDB {

    std::map<block_id, ptr<CommittedBlock>> blocks;

    uint64_t storageSize;

    recursive_mutex mutex;

    ptr<string> createKey(block_id _blockId);

    const string getFormatVersion();


    void saveBlockToBlockCache(ptr<CommittedBlock> &_block, block_id _lastCommittedBlockID);

    void saveBlock2LevelDB(ptr<CommittedBlock> &_block);

    ptr<CommittedBlock> getCachedBlock(block_id _blockID);

public:

    BlockDB(string &_filename, node_id _nodeId, uint64_t _storageSize);

    ptr<vector<uint8_t >> getSerializedBlockFromLevelDB(block_id _blockID);

    uint64_t readCounter();

    void saveBlock(ptr<CommittedBlock> &_block, block_id _lastCommittedBlockID);


    ptr<CommittedBlock> getBlock(block_id _blockID, ptr<CryptoManager> _cryptoManager);
};


#endif //SKALED_BLOCKDB_H
