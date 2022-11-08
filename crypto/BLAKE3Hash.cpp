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

    @file BLAKE3Hash.cpp
    @author Stan Kladko
    @date 2019
*/

#include "SkaleCommon.h"
#include "Log.h"

#include "thirdparty/json.hpp"


#include "messages/Message.h"
#include "network/Utils.h"
#include "exceptions/InvalidArgumentException.h"

#include "BLAKE3Hash.h"

void BLAKE3Hash::print() {
    for (size_t i = 0; i < HASH_LEN; i++) {
        cerr << to_string(hash.at(i));
    }
}


uint8_t BLAKE3Hash::at(uint32_t _position) {
    return hash.at(_position);
}


BLAKE3Hash BLAKE3Hash::fromHex(const string& _hex) {
    CHECK_ARGUMENT(_hex != "");
    BLAKE3Hash result;
    Utils::cArrayFromHex(_hex, result.data(), HASH_LEN);
    return result;
}

string BLAKE3Hash::toHex() {
    auto result = Utils::carray2Hex(hash.data(), HASH_LEN);
    CHECK_STATE(result != "");
    return result;
}


int BLAKE3Hash::compare(BLAKE3Hash& _hash2 ) {


    for (size_t i = 0; i < HASH_LEN; i++) {
        if (hash.at(i) < _hash2.at(i))
            return -1;
        if (hash.at(i) > _hash2.at(i))
            return 1;
    }
    return 0;
}


BLAKE3Hash BLAKE3Hash::calculateHash(const ptr<vector<uint8_t>>& _data) {
    CHECK_ARGUMENT(_data);
    // Initialize the hasher.

    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, _data->data(), _data->size());
    BLAKE3Hash hash;
    blake3_hasher_finalize(&hasher, hash.data(), BLAKE3_OUT_LEN);
    return hash;
}

BLAKE3Hash BLAKE3Hash::merkleTreeMerge(const BLAKE3Hash & _left, const BLAKE3Hash& _right) {


    auto concatenation = make_shared<vector<uint8_t>>();
    concatenation->reserve(2 * HASH_LEN);

    auto leftHash = _left.getHash();

    concatenation->insert(concatenation->end(), leftHash.begin(), leftHash.end());

    auto rightHash = _right.getHash();

    concatenation->insert(concatenation->end(), rightHash.begin(), rightHash.end());

    return calculateHash(concatenation);
}

const array<uint8_t, HASH_LEN>& BLAKE3Hash::getHash() const {
    return hash;
}



BLAKE3Hash BLAKE3Hash::getConsensusHash(uint64_t _blockProposerIndex, uint64_t _blockId, uint64_t _schainId) {
    uint32_t msgType = MSG_BLOCK_SIGN_BROADCAST;
    BLAKE3Hash _hash;
    HASH_INIT(hashObj)
    HASH_UPDATE(hashObj, _blockProposerIndex)
    HASH_UPDATE(hashObj, _blockId)
    HASH_UPDATE(hashObj, _schainId)
    HASH_UPDATE(hashObj, msgType);
    HASH_FINAL(hashObj, _hash.data());
    return _hash;
}