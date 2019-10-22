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

    @file ProposalHashDB.cpp
    @author Stan Kladko
    @date 2019
*/


#include "../SkaleCommon.h"
#include "../Log.h"
#include "../chains/Schain.h"
#include "../exceptions/InvalidStateException.h"
#include "../datastructures/CommittedBlock.h"

#include "ProposalHashDB.h"

ptr<vector<uint8_t> > ProposalHashDB::getSerializedBlockFromLevelDB(block_id _blockID) {

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

ProposalHashDB::ProposalHashDB(string &_filename, node_id _nodeId, uint64_t _storageSize) : LevelDB(_filename, _nodeId) {

    CHECK_ARGUMENT(_storageSize != 0);
    storageSize = _storageSize;
}


void ProposalHashDB::saveBlock2LevelDB(ptr<CommittedBlock> &_block) {


    CHECK_ARGUMENT(_block->getSignature() != nullptr);

    lock_guard<recursive_mutex> lock(mutex);

    try {

        auto serializedBlock = _block->getSerialized();

        auto key = createKey(_block->getBlockID());

        auto value = (const char *) serializedBlock->data();

        auto valueLen = serializedBlock->size();

        writeByteArray(*key, value, valueLen);
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}

ptr<string> ProposalHashDB::createKey(const block_id _blockId) {
    return make_shared<string>(getFormatVersion() + ":" + to_string(nodeId) + ":" + to_string(_blockId));
}

const string ProposalHashDB::getFormatVersion() {
    return "1.0";
}

uint64_t ProposalHashDB::readCounter() {

    static string count(":COUNT");


    lock_guard<recursive_mutex> lock(mutex);

    try {

        auto key = getFormatVersion() + count;

        auto value = readString(key);

        if (value != nullptr) {
            return stoul(*value);
        } else {
            return 0;
        }
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


void ProposalHashDB::saveBlock(ptr<CommittedBlock> &_block, block_id _lastCommittedBlockID) {


    CHECK_ARGUMENT(_block->getSignature() != nullptr);

    try {
        lock_guard<recursive_mutex> lock(mutex);

        saveBlockToBlockCache(_block, _lastCommittedBlockID);
        saveBlock2LevelDB(_block);

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}



ptr<CommittedBlock> ProposalHashDB::getBlock(block_id _blockID, ptr<CryptoManager> _cryptoManager) {


    std::lock_guard<std::recursive_mutex> lock(mutex);

    try {

        auto serializedBlock = getSerializedBlockFromLevelDB(_blockID);

        if (serializedBlock == nullptr) {
            return nullptr;
        }

        return CommittedBlock::deserialize(serializedBlock, _cryptoManager);
    }

    catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}