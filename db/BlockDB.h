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

class BlockDB : public LevelDB{


    ptr<string>  createKey(block_id _blockId);

    const string getFormatVersion();

public:

    BlockDB(string& _filename, node_id _nodeId);
    ptr<vector<uint8_t >> getSerializedBlock( block_id _blockID );

    void saveBlock(ptr<CommittedBlock> &_block);

    uint64_t readCounter();

};



#endif //SKALED_BLOCKDB_H
