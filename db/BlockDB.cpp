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
#include "datastructures/CommittedBlock.h"
#include "exceptions/InvalidStateException.h"
#include "utils/Time.h"

#include "BlockDB.h"

ptr<vector<uint8_t> > BlockDB::getSerializedBlockFromLevelDB(block_id _blockID) {

    shared_lock<shared_mutex> lock(m);

    try {

        auto key = createKey(_blockID);
        CHECK_STATE(!key.empty())
        auto value = readString(key);

        if (!value.empty()) {
            auto serializedBlock = make_shared<vector<uint8_t>>();
            serializedBlock->insert(serializedBlock->begin(), value.data(), value.data() + value.size());
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


void BlockDB::saveBlock2LevelDB(const ptr<CommittedBlock> &_block) {

    CHECK_ARGUMENT(_block)
    CHECK_ARGUMENT(!_block->getSignature().empty())

    lock_guard<shared_mutex> lock(m);

    try {

        auto serializedBlock = _block->serializeBlock();

        CHECK_STATE(serializedBlock)

        auto key = createKey(_block->getBlockID());
        CHECK_STATE(!key.empty())
        writeByteArray(key, serializedBlock);
        writeString(createLastCommittedKey(), to_string(_block->getBlockID()), true);
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}




string BlockDB::createLastCommittedKey() {
    return getFormatVersion() + ":last";
}

string BlockDB::createBlockStartKey( block_id _blockID) {
    return getFormatVersion() + ":start:" + to_string((uint64_t) _blockID);
}

const string& BlockDB::getFormatVersion() {
    static const string version = "1.0";
    return version;
}

void BlockDB::saveBlock(const ptr<CommittedBlock> &_block) {

    CHECK_ARGUMENT(_block)
    CHECK_ARGUMENT(!_block->getSignature().empty())

    try {
        saveBlock2LevelDB(_block);
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}


ptr<CommittedBlock> BlockDB::getBlock(block_id _blockID, const ptr<CryptoManager>& _cryptoManager) {

    CHECK_ARGUMENT(_cryptoManager)

    shared_lock<shared_mutex> lock(m);

    try {

        auto serializedBlock = getSerializedBlockFromLevelDB(_blockID);

        if (serializedBlock == nullptr) {
            LOG(debug, "Got null block in BlockDB::getBlock");
            return nullptr;
        }

        // dont check signatures on blocks that are already in internal db
        // they have already been verified
        auto result = CommittedBlock::deserialize(serializedBlock, _cryptoManager, false);
        CHECK_STATE(result)
        return result;
    }

    catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}

block_id BlockDB::readLastCommittedBlockID() {

    shared_lock<shared_mutex> lock(m);

    uint64_t  lastBlockId;

    auto key = createLastCommittedKey();

    auto blockStr = readString(key);

    if (blockStr.empty())
        return 0;

    stringstream(blockStr)  >> lastBlockId;

    return lastBlockId;
}

bool BlockDB::unfinishedBlockExists( block_id  _blockID) {

    shared_lock<shared_mutex> lock(m);

    auto key = createBlockStartKey(_blockID);
    auto str = readString(key);

    if (!str.empty()) {
        return !this->keyExists(createKey(_blockID));
    }
    return false;
}


