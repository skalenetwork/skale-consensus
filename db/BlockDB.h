//
// Created by kladko on 7/12/19.
//

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

};



#endif //SKALED_BLOCKDB_H
