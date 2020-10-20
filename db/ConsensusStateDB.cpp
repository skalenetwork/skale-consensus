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

#include "chains/Schain.h"
#include "crypto/CryptoManager.h"
#include "datastructures/Transaction.h"

#include "ConsensusStateDB.h"

_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wunused-parameter\"")


ConsensusStateDB::ConsensusStateDB(Schain *_sChain, string &_dirName, string &_prefix, node_id _nodeId,
                                   uint64_t _maxDBSize) : CacheLevelDB(_sChain, _dirName, _prefix, _nodeId,
                                                                       _maxDBSize, false) {}


const string ConsensusStateDB::getFormatVersion() {
    return "1.0";
}


ptr<string> ConsensusStateDB::createCurrentRoundKey(block_id _blockId, schain_index _proposerIndex) {
    auto key = createKey(_blockId, _proposerIndex);
    CHECK_STATE(key);
    key->append(":cr");
    return key;
}

ptr<string> ConsensusStateDB::createDecidedRoundKey(block_id _blockId, schain_index _proposerIndex) {
    auto key = createKey(_blockId, _proposerIndex);
    CHECK_STATE(key);
    key->append(":dr");
    return key;

}

ptr<string> ConsensusStateDB::createDecidedValueKey(block_id _blockId, schain_index _proposerIndex) {
    auto key = createKey(_blockId, _proposerIndex);
    CHECK_STATE(key);
    key->append(":dv");
    return key;
}


ptr<string>
ConsensusStateDB::createProposalKey(block_id _blockId, schain_index _proposerIndex, bin_consensus_round _r) {
    auto key = createKey(_blockId, _proposerIndex);
    CHECK_STATE(key);
    key->append(":prp:").append(to_string(_r));
    return key;
}

ptr<string>
ConsensusStateDB::createBVBVoteKey(block_id _blockId, schain_index _proposerIndex, bin_consensus_round _r,
                                   schain_index _voterIndex, bin_consensus_value _v) {
    auto key = createKey(_blockId, _proposerIndex);
    CHECK_STATE(key);
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
#ifdef CONSENSUS_STATE_PERSISTENCE
    auto key = createCurrentRoundKey(_blockId, _proposerIndex);
    writeString(*key, to_string((uint64_t) _r), true);
#endif
}

bin_consensus_round ConsensusStateDB::readCR(block_id _blockId, schain_index _proposerIndex) {
    auto key = createCurrentRoundKey(_blockId, _proposerIndex);
    auto round = readString(*key);
    if (round == nullptr) {
        return 0;
    }

    uint64_t result;
    stringstream(*round) >> result;
    return result;
}

void ConsensusStateDB::writeDR(block_id _blockId, schain_index _proposerIndex, bin_consensus_round _r) {
#ifdef CONSENSUS_STATE_PERSISTENCE
    auto key = createDecidedRoundKey(_blockId, _proposerIndex);
    writeString(*key, to_string((uint64_t) _r));
#endif
}

pair<bool, bin_consensus_round> ConsensusStateDB::readDR(block_id _blockId, schain_index _proposerIndex) {
    auto key = createDecidedRoundKey(_blockId, _proposerIndex);
    auto value = readString(*key);
    if (value == nullptr) {
        return {false, 0};
    }
    uint64_t result;
    stringstream(*value) >> result;
    return {true, result};
}

void ConsensusStateDB::writeDV(block_id _blockId, schain_index _proposerIndex, bin_consensus_value _v) {
#ifdef CONSENSUS_STATE_PERSISTENCE
    CHECK_ARGUMENT(_v <= 1)

    auto key = createDecidedValueKey(_blockId, _proposerIndex);
    writeString(*key, to_string((uint32_t) (uint8_t) _v));
#endif
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
#ifdef CONSENSUS_STATE_PERSISTENCE
    CHECK_ARGUMENT(_v <= 1)
    auto key = createProposalKey(_blockId, _proposerIndex, _r);
    writeString(*key, to_string((uint32_t) (uint8_t) _v));
#endif
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
#ifdef CONSENSUS_STATE_PERSISTENCE
    CHECK_ARGUMENT(_v <= 1)
    auto key = createBVBVoteKey(_blockId, _proposerIndex, _r, _voterIndex, _v);
    writeString(*key, "");
#endif

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
#ifdef CONSENSUS_STATE_PERSISTENCE
    CHECK_ARGUMENT(_v <= 1)
    auto key = createBinValueKey(_blockId, _proposerIndex, _r, _v);
    writeString(*key, "");
#endif
}

ptr<map<bin_consensus_round, set<bin_consensus_value>>>
ConsensusStateDB::readBinValues(block_id _blockId, schain_index _proposerIndex) {

    auto result = make_shared<map<bin_consensus_round, set<bin_consensus_value>>>();

    auto prefix = createKey(_blockId, _proposerIndex)->append(":bin:");
    auto keysAndValues = readPrefixRange(prefix);

    if (keysAndValues == nullptr) {
        return result;
    }

    for (auto&& item : *keysAndValues) {
        CHECK_STATE(item.first.rfind(prefix) == 0);


        auto info = stringstream(item.first.substr(prefix.size()));
        uint64_t round;
        uint32_t value;
        info >> round;
        CHECK_STATE(info.get() == ':');
        info >> value;
        bin_consensus_value b(value > 0 ? 1 : 0);
        (*result)[bin_consensus_round(round)].insert(b);
    }
    return result;
}

ptr<map  <bin_consensus_round, bin_consensus_value>>
ConsensusStateDB::readPRs(block_id _blockId, schain_index _proposerIndex) {

    auto result = make_shared<map<bin_consensus_round, bin_consensus_value>>();

    auto prefix = createKey(_blockId, _proposerIndex)->append(":pr:");
    auto keysAndValues = readPrefixRange(prefix);

    if (keysAndValues == nullptr) {
        return result;
    }

    for (auto&& item : *keysAndValues) {
        CHECK_STATE(item.first.rfind(prefix) == 0);


        auto info = stringstream(item.first.substr(prefix.size()));
        uint64_t round;
        uint32_t value;
        info >> round;
        CHECK_STATE(info.get() == ':');
        info >> value;
        bin_consensus_value b(value > 0 ? 1 : 0);
        (*result)[bin_consensus_round(round)] = b;
    }
    return result;
}



void ConsensusStateDB::writeAUXVote(block_id _blockId, schain_index _proposerIndex, bin_consensus_round _r,
                                    schain_index _voterIndex,
                                    bin_consensus_value _v, const ptr<string>& _sigShare) {
#ifdef CONSENSUS_STATE_PERSISTENCE
    CHECK_ARGUMENT(_v <= 1);
    CHECK_ARGUMENT(_sigShare);
    auto key = createAUXVoteKey(_blockId, _proposerIndex, _r, _voterIndex, _v);
    writeString(*key, *_sigShare);
#endif

}

pair<ptr<map<bin_consensus_round, map<schain_index, ptr<ThresholdSigShare>>>>,
        ptr<map<bin_consensus_round, map<schain_index, ptr<ThresholdSigShare>>>>>
ConsensusStateDB::readAUXVotes(block_id _blockId, schain_index _proposerIndex, const ptr<CryptoManager>& _cryptoManager) {

    CHECK_ARGUMENT(_cryptoManager);

    auto trueMap = make_shared<map<bin_consensus_round, map<schain_index, ptr<ThresholdSigShare>>>>();
    auto falseMap = make_shared<map<bin_consensus_round, map<schain_index, ptr<ThresholdSigShare>>>>();


    auto key = createKey(_blockId, _proposerIndex);
    CHECK_STATE(key);

    auto prefix = key->append(":aux:");

    auto keysAndValues = readPrefixRange(prefix);

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

        ptr<map<bin_consensus_round, map<schain_index, ptr<ThresholdSigShare>>>> outputMap;
        outputMap = (value > 0  ? trueMap : falseMap);

        (*outputMap)[bin_consensus_round(round)][schain_index(voterIndex)] =
                _cryptoManager->createSigShare(item.second,
                        getSchain()->getSchainID(), _blockId, voterIndex);
    }

    return {trueMap, falseMap};
}










