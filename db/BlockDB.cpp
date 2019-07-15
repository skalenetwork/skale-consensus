//
// Created by kladko on 7/12/19.
//

#include "../SkaleCommon.h"
#include "../Log.h"

#include "../datastructures/CommittedBlock.h"

#include "BlockDB.h"


ptr<vector<uint8_t> > BlockDB::getSerializedBlock( block_id _blockID ) {

    auto key = createKey(_blockID);

    auto value = readString(*key);

    if (value) {
        auto serializedBlock = make_shared<vector<uint8_t>>();
        serializedBlock->insert(serializedBlock->begin(), value->data(), value->data() + value->size());
        return serializedBlock;
    } else {
        return nullptr;
    }
}

BlockDB::BlockDB(string& filename, node_id _nodeId ) : LevelDB( filename, _nodeId ) {}


void BlockDB::saveBlock(ptr<CommittedBlock> &_block) {
    auto serializedBlock = _block->serialize();


    auto key = createKey(_block->getBlockID() );

    auto value = (const char *) serializedBlock->data();

    auto valueLen = serializedBlock->size();


    writeByteArray(*key, value, valueLen);

}

ptr<string>  BlockDB::createKey(const block_id _blockId) {
    return make_shared<string>(getFormatVersion() + ":" + to_string( nodeId ) + ":" + to_string( _blockId ));
}
const string BlockDB::getFormatVersion() {
    return "1.0";
}
