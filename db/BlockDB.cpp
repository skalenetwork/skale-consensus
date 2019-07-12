//
// Created by kladko on 7/12/19.
//

#include "../SkaleCommon.h"
#include "../Log.h"

#include "BlockDB.h"


ptr<vector<uint8_t>> BlockDB::getSerializedBlock(node_id _nodeId, block_id _blockID) {
    using namespace leveldb;

    string key = to_string((uint64_t) _nodeId) + ":" + to_string((uint64_t) _blockID);

    auto value = readString(key);

    if (value) {
        auto serializedBlock = make_shared<vector<uint8_t>>();
        serializedBlock->insert(serializedBlock->begin(), value->data(), value->data() + value->size());
        return serializedBlock;
    } else {
        return nullptr;
    }
}
BlockDB::BlockDB( string& filename ) : LevelDB( filename ) {}
