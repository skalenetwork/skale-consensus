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

    @file BlockDB.cpp
    @author Stan Kladko
    @date 2019
*/


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


void BlockDB::saveBlock2LevelDB(ptr<CommittedBlock> &_block) {

    lock_guard<recursive_mutex> lock(mutex);

    auto serializedBlock = _block->getSerialized();


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
uint64_t BlockDB::readCounter(){

    static string count(":COUNT");


    lock_guard<recursive_mutex> lock(mutex);

    auto key = getFormatVersion() + count;

    auto value = readString(key);

    if (value != nullptr) {
        return stoul(*value);
    } else {
        return 0;
    }

}
