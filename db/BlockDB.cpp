//
// Created by kladko on 7/12/19.
//

#include "../SkaleCommon.h"
#include "../Log.h"

#include "../datastructures/CommittedBlock.h"

#include "BlockDB.h"


ptr<vector<uint8_t> > BlockDB::getSerializedBlock( block_id _blockID ) {
    using namespace leveldb;

    string key = to_string((uint64_t) nodeId) + ":" + to_string((uint64_t) _blockID);

    auto value = readString(key);

    if (value) {
        auto serializedBlock = make_shared<vector<uint8_t>>();
        serializedBlock->insert(serializedBlock->begin(), value->data(), value->data() + value->size());
        return serializedBlock;
    } else {
        return nullptr;
    }
}
BlockDB::BlockDB(node_id _nodeId,  string& filename ) : LevelDB( filename ),  nodeId(_nodeId) {}


void BlockDB::saveBlock(ptr<CommittedBlock> &_block) {
    auto serializedBlock = _block->serialize();

    using namespace leveldb;

    auto key = to_string(nodeId) + ":"
               + to_string(_block->getBlockID());

    auto value = (const char *) serializedBlock->data();

    auto valueLen = serializedBlock->size();


    writeByteArray(key, value, valueLen);

}