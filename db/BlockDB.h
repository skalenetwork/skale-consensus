//
// Created by kladko on 7/12/19.
//

#ifndef SKALED_BLOCKDB_H
#define SKALED_BLOCKDB_H

#include "LevelDB.h"

class BlockDB : public LevelDB{

public:
    BlockDB( string& filename );
    ptr<vector<uint8_t>> getSerializedBlock(node_id _nodeId, block_id _blockID);

};



#endif //SKALED_BLOCKDB_H
