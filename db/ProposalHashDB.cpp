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
#include "../crypto/SHAHash.h"
#include "../chains/Schain.h"
#include "../exceptions/InvalidStateException.h"
#include "../datastructures/CommittedBlock.h"

#include "ProposalHashDB.h"


ProposalHashDB::ProposalHashDB(string &_filename, node_id _nodeId, uint64_t _blockIdsPerDB) : LevelDB(_filename,
                                                                                                      _nodeId) {
    CHECK_ARGUMENT(_blockIdsPerDB != 0);
    blockIdsPerDB = _blockIdsPerDB;
}


bool ProposalHashDB::checkAndSaveHash(ptr<BlockProposal> &_block, block_id /*_lastCommittedBlockID*/) {


    lock_guard<recursive_mutex> lock(mutex);

    try {

        auto hexHash = _block->getHash()->toHex();

        auto key = createKey(_block->getBlockID(), _block->getProposerIndex());

        auto previous = readString(*key);

        if (previous == nullptr) {
            writeString(*key, *hexHash);
            return true;
        }

        return (*previous == *hexHash);

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}

ptr<string> ProposalHashDB::createKey(block_id _blockId, schain_index _proposerIndex) {
    return make_shared<string>(getFormatVersion() + ":" + to_string(_blockId) + ":" + to_string(_proposerIndex));
}

const string ProposalHashDB::getFormatVersion() {
    return "1.0";
}

uint64_t ProposalHashDB::readBlockLimit() {

    static string count(":MAX_BLOCK_ID");


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





