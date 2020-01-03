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


ptr<string> ConsensusStateDB::createCurrentRoundKey(block_id _blockId, schain_index _proposerIndex) {

    auto key = createKey(_blockId, _proposerIndex);
    key->append(":cr");
    return key;
}

ptr<string> ConsensusStateDB::createDecidedRoundKey(block_id _blockId, schain_index _proposerIndex) {
    auto key = createKey(_blockId, _proposerIndex);
    key->append(":dr");
    return key;

}

ptr<string> ConsensusStateDB::createDecidedValueKey(block_id _blockId, schain_index _proposerIndex) {
    auto key = createKey(_blockId, _proposerIndex);
    key->append(":dv");
    return key;
}


ptr<string>
ConsensusStateDB::createProposalKey(block_id _blockId, schain_index _proposerIndex, bin_consensus_round _r) {
    auto key = createKey(_blockId, _proposerIndex);
    key->append(":prp:").append(to_string(_r));
    return key;
}

ptr<string>
ConsensusStateDB::createBVBVoteKey(block_id _blockId, schain_index _proposerIndex, bin_consensus_round _r,
                                   schain_index _voterIndex, bin_consensus_value _v) {
    auto key = createKey(_blockId, _proposerIndex);
    key->
            append(":bvb:").append(to_string(_r)).append(":").append(to_string(_voterIndex)).append(":").append(
            to_string(
                    (uint32_t) (uint8_t) _v));
    return key;
}


ptr<string> ConsensusStateDB::createBinValueKey(block_id _blockId, schain_index _proposerIndex, bin_consensus_round _r,
                                                bin_consensus_value _v) {
    auto key = createKey(_blockId, _proposerIndex);
    key->append(":bin:").append(to_string(_r)).append(":").append(to_string((uint32_t) (uint8_t) _v));
    return key;
}

ptr<string>
ConsensusStateDB::createAUXVoteKey(block_id _blockId, schain_index _proposerIndex, bin_consensus_round _r,
                                   schain_index _voterIndex, bin_consensus_value _v) {

    auto key = createKey(_blockId, _proposerIndex);
    key->
            append(":aux:").append(to_string(_r)).append(":").append(to_string(_voterIndex)).append(":").append(
            to_string(
                    (uint32_t) (uint8_t) _v));
    return key;
}


void ConsensusStateDB::writeCR(block_id _blockId, schain_index _proposerIndex, bin_consensus_round _r) {
    auto key = createCurrentRoundKey(_blockId, _proposerIndex);
    writeString(*key, to_string((uint64_t) _r), true);
    assert(readCR(_blockId, _proposerIndex) == _r);
}

bin_consensus_round ConsensusStateDB::readCR(block_id _blockId, schain_index _proposerIndex) {
    auto key = createCurrentRoundKey(_blockId, _proposerIndex);
    auto round = readString(*key);
    if (round == nullptr)
        BOOST_THROW_EXCEPTION(InvalidStateException("Missing CR", __CLASS_NAME__));
    uint64_t result;
    stringstream(*round) >> result;
    return result;
}

void ConsensusStateDB::writeDR(block_id _blockId, schain_index _proposerIndex, bin_consensus_round _r) {
    auto key = createDecidedRoundKey(_blockId, _proposerIndex);
    writeString(*key, to_string((uint64_t) _r));
    assert(readDR(_blockId, _proposerIndex) == _r);

}

bin_consensus_round ConsensusStateDB::readDR(block_id _blockId, schain_index _proposerIndex) {
    auto key = createDecidedRoundKey(_blockId, _proposerIndex);
    auto value = readString(*key);
    if (value == nullptr)
        BOOST_THROW_EXCEPTION(InvalidStateException("Missing DR", __CLASS_NAME__));
    uint64_t result;
    stringstream(*value) >> result;
    return result;
}

void ConsensusStateDB::writeDV(block_id _blockId, schain_index _proposerIndex, bin_consensus_value _v) {
    CHECK_ARGUMENT(_v <= 1)

    auto key = createDecidedValueKey(_blockId, _proposerIndex);
    writeString(*key, to_string((uint32_t) (uint8_t) _v));
    assert(readDV(_blockId, _proposerIndex) == _v);
}

bin_consensus_value ConsensusStateDB::readDV(block_id _blockId, schain_index _proposerIndex) {
    auto key = createDecidedValueKey(_blockId, _proposerIndex);
    auto value = readString(*key);
    if (value == nullptr)
        BOOST_THROW_EXCEPTION(InvalidStateException("Missing DV", __CLASS_NAME__));
    uint32_t result;
    stringstream(*value) >> result;

    return (uint8_t) result;
}

void ConsensusStateDB::writePr(block_id _blockId, schain_index _proposerIndex, bin_consensus_round _r,
                               bin_consensus_value _v) {
    CHECK_ARGUMENT(_v <= 1)
    auto key = createProposalKey(_blockId, _proposerIndex, _r);
    writeString(*key, to_string((uint32_t) (uint8_t) _v));
    assert(readPR(_blockId, _proposerIndex, _r) == _v);
}

bin_consensus_value ConsensusStateDB::readPR(block_id _blockId, schain_index _proposerIndex,
                                             bin_consensus_round _r) {
    auto key = createProposalKey(_blockId, _proposerIndex, _r);
    auto value = readString(*key);
    if (value == nullptr)
        BOOST_THROW_EXCEPTION(InvalidStateException("Missing DV", __CLASS_NAME__));
    uint32_t result;
    stringstream(*value) >> result;
    return (uint8_t) result;
}


void ConsensusStateDB::writeBVBVote(block_id _blockId, schain_index _proposerIndex, bin_consensus_round _r,
                                    schain_index _voterIndex, bin_consensus_value _v) {
    CHECK_ARGUMENT(_v <= 1)
    auto key = createBVBVoteKey(_blockId, _proposerIndex, _r, _voterIndex, _v);
    writeString(*key, "");
    auto saved = readBVBVotes(_blockId, _proposerIndex);
    auto x = (*(_v > 0 ? saved.first : saved.second))[_r];
    CHECK_STATE(x.find(_voterIndex) != x.end());
}

pair<ptr<map<bin_consensus_round, set<schain_index>>>,
        ptr<map<bin_consensus_round, set<schain_index>>>>
ConsensusStateDB::readBVBVotes(block_id _blockId, schain_index _proposerIndex) {

    auto prefix = createKey(_blockId, _proposerIndex)->append(":bvb:");
    auto keysAndValues = readPrefixRange(prefix);

    auto trueMap = make_shared<map<bin_consensus_round, set<schain_index>>>();
    auto falseMap = make_shared<map<bin_consensus_round, set<schain_index>>>();


    if (keysAndValues == nullptr) {
        return {trueMap, falseMap};
    }

    for (auto&& item : *keysAndValues) {
        CHECK_STATE(item.first.rfind(prefix) == 0);
        auto info = stringstream(item.first.substr(prefix.size()));
        uint64_t round;
        uint64_t voterIndex;
        uint32_t value;
        info >> round;
        CHECK_STATE(info.get() == ':');
        info >> voterIndex;
        CHECK_STATE(info.get() == ':');
        info >> value;

        ptr<map<bin_consensus_round, set<schain_index>>> outputMap;
        outputMap = (value > 0  ? trueMap : falseMap);
        (*outputMap)[bin_consensus_round(round)].insert(schain_index(voterIndex));
    }

    return {trueMap, falseMap};
}


void ConsensusStateDB::writeBinValue(block_id _blockId, schain_index _proposerIndex, bin_consensus_round _r,
                                     bin_consensus_value _v) {
    CHECK_ARGUMENT(_v <= 1)
    auto key = createBinValueKey(_blockId, _proposerIndex, _r, _v);
    writeString(*key, "");
}


void ConsensusStateDB::writeAUXVote(block_id _blockId, schain_index _proposerIndex, bin_consensus_round _r,
                                    schain_index _voterIndex,
                                    bin_consensus_value _v, ptr<string> _sigShare) {
    CHECK_ARGUMENT(_v <= 1);
    CHECK_ARGUMENT(_sigShare);
    auto key = createAUXVoteKey(_blockId, _proposerIndex, _r, _voterIndex, _v);
    writeString(*key, *_sigShare);

    auto saved = readAUXVotes(_blockId, _proposerIndex);
    auto x = (*(_v > 0 ? saved.first : saved.second))[_r];
    CHECK_STATE(x.find(_voterIndex) != x.end());
    auto sig = x.at(_voterIndex);
    CHECK_STATE(*x.at(_voterIndex) == *_sigShare);
}

pair<ptr<map<bin_consensus_round, map<schain_index, ptr<string>>>>,
        ptr<map<bin_consensus_round, map<schain_index, ptr<string>>>>>
ConsensusStateDB::readAUXVotes(block_id _blockId, schain_index _proposerIndex) {
    auto prefix = createKey(_blockId, _proposerIndex)->append(":aux:");
    auto keysAndValues = readPrefixRange(prefix);

    auto trueMap = make_shared<map<bin_consensus_round, map<schain_index, ptr<string>>>>();
    auto falseMap = make_shared<map<bin_consensus_round, map<schain_index, ptr<string>>>>();

    if (keysAndValues == nullptr) {
        return {trueMap, falseMap};
    }

    for (auto&& item : *keysAndValues) {
        CHECK_STATE(item.first.rfind(prefix) == 0);
        auto info = stringstream(item.first.substr(prefix.size()));
        uint64_t round;
        uint64_t voterIndex;
        uint32_t value;
        info >> round;
        CHECK_STATE(info.get() == ':');
        info >> voterIndex;
        CHECK_STATE(info.get() == ':');
        info >> value;

        ptr<map<bin_consensus_round, map<schain_index, ptr<string>>>> outputMap;
        outputMap = (value > 0  ? trueMap : falseMap);
        (*outputMap)[bin_consensus_round(round)][schain_index(voterIndex)] = item.second;
    }

    return {trueMap, falseMap};
}









