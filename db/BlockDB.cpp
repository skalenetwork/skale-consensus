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


#include "SkaleCommon.h"
#include "Log.h"
#include "chains/Schain.h"
#include "exceptions/InvalidStateException.h"
#include "datastructures/CommittedBlock.h"

#include "BlockDB.h"
#include "CacheLevelDB.h"

ptr<vector<uint8_t> > BlockDB::getSerializedBlockFromLevelDB(block_id _blockID) {

    try {

        auto key = createKey(_blockID);

        auto value = readString(*key);

        if (value) {
            auto serializedBlock = make_shared<vector<uint8_t>>();
            serializedBlock->insert(serializedBlock->begin(), value->data(), value->data() + value->size());
            CommittedBlock::serializedSanityCheck(serializedBlock);
            return serializedBlock;
        } else {
            return nullptr;
        }
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}

BlockDB::BlockDB(Schain *_sChain, string &_dirname, string &_prefix, node_id _nodeId, uint64_t _maxDBSize)
        : CacheLevelDB(_sChain, _dirname, _prefix,
                       _nodeId, _maxDBSize, false) {


}


void BlockDB::saveBlock2LevelDB(ptr<CommittedBlock> &_block) {

    CHECK_ARGUMENT(_block->getSignature() != nullptr);

    LOCK(m)

    try {

        auto serializedBlock = _block->serialize();

        auto key = createKey(_block->getBlockID());
        
        writeByteArray(*key, serializedBlock);
        writeString(createLastCommittedKey(), to_string(_block->getBlockID()), true);
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}




string BlockDB::createLastCommittedKey() {
    return getFormatVersion() + ":last";
}


const string BlockDB::getFormatVersion() {
    return "1.0";
}


void BlockDB::saveBlock(ptr<CommittedBlock> &_block) {


    CHECK_ARGUMENT(_block->getSignature() != nullptr);

    try {
        LOCK(m)
        saveBlock2LevelDB(_block);

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}


ptr<CommittedBlock> BlockDB::getBlock(block_id _blockID, ptr<CryptoManager> _cryptoManager) {


    std::lock_guard<std::recursive_mutex> lock(m);

    try {

        auto serializedBlock = getSerializedBlockFromLevelDB(_blockID);

        if (serializedBlock == nullptr) {
            cerr << "got null" << endl;
            return nullptr;
        }

        return CommittedBlock::deserialize(serializedBlock, _cryptoManager);
    }

    catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}

block_id BlockDB::readLastCommittedBlockID() {

    uint64_t  lastBlockId;

    auto key = createLastCommittedKey();

    auto blockStr = readString(key);

    if (!blockStr)
        return 0;

    stringstream(*blockStr)  >> lastBlockId;

    return lastBlockId;
}
