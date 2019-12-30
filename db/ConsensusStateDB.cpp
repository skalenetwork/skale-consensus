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

    @file ConsensusStateDB.cpp
    @author Stan Kladko
    @date 2019
*/


#include "SkaleCommon.h"
#include "Log.h"

#include "datastructures/Transaction.h"

#include "ConsensusStateDB.h"


ConsensusStateDB::ConsensusStateDB(Schain *_sChain, string &_dirName, string &_prefix, node_id _nodeId,
                                               uint64_t _maxDBSize) : CacheLevelDB(_sChain, _dirName, _prefix, _nodeId,
                                                                                   _maxDBSize, false) {}


const string ConsensusStateDB::getFormatVersion() {
    return "1.0";
}

ptr<string> ConsensusStateDB::createCurrentRoundKey(block_id _blockId) {
    return make_shared<string>(
            getFormatVersion() + ":" + to_string(_blockId) + ":cr");
}

ptr<string> ConsensusStateDB::createDecidedRoundKey(block_id _blockId) {
    return make_shared<string>(
            getFormatVersion() + ":" + to_string(_blockId) + ":dr");
}

ptr<string> ConsensusStateDB::createDecidedValueKey(block_id _blockId) {
    return make_shared<string>(
            getFormatVersion() + ":" + to_string(_blockId) + ":dv");
}


ptr<string> ConsensusStateDB::createProposalKey(block_id _blockId, bin_consensus_round _r) {
    return make_shared<string>(
            getFormatVersion() + ":" + to_string(_blockId) + ":pr:" + to_string(_r));

}


void ConsensusStateDB::writeCR(block_id _blockId, bin_consensus_round _r) {
    auto key = createCurrentRoundKey(_blockId);
    writeString(*key, to_string((uint64_t) _r), true);
}

void ConsensusStateDB::writeDR(block_id _blockId, bin_consensus_round _r) {
    auto key = createDecidedRoundKey(_blockId);
    writeString(*key, to_string((uint64_t) _r));
}

void ConsensusStateDB::writeDV(block_id _blockId, bin_consensus_value _v) {
    CHECK_ARGUMENT(_v <= 1 )
    auto key = createDecidedValueKey(_blockId);
    writeString(*key, to_string((uint8_t) _v));
}

void ConsensusStateDB::writePr(block_id _blockId, bin_consensus_round _r, bin_consensus_value _v) {
    CHECK_ARGUMENT(_v <= 1 )
    auto key = createProposalKey(_blockId, _r);
    writeString(*key, to_string((uint8_t) _v));
}







