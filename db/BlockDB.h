//
// Created by kladko on 7/12/19.
//

#ifndef SKALED_BLOCKDB_H
#define SKALED_BLOCKDB_H

class CommittedBlock;

#include "LevelDB.h"

class BlockDB : public LevelDB{

    node_id nodeId;

public:
    BlockDB(node_id nodeId,  string& filename );
    ptr<vector<uint8_t >> getSerializedBlock( block_id _blockID );

    void saveBlock(ptr<CommittedBlock> &_block);

};



#endif //SKALED_BLOCKDB_H
