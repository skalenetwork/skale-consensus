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


#include "network/Utils.h"
#include "exceptions/InvalidArgumentException.h"

#include "BLAKE3Hash.h"

void BLAKE3Hash::print() {
    CHECK_STATE(hash);
    for (size_t i = 0; i < SHA_HASH_LEN; i++) {
        cerr << to_string(hash->at(i));
    }
}


uint8_t BLAKE3Hash::at(uint32_t _position) {
    CHECK_STATE(hash);
    return hash->at(_position);
}


ptr<BLAKE3Hash> BLAKE3Hash::fromHex(const string& _hex) {
    CHECK_ARGUMENT(_hex != "");
    auto result = make_shared<array<uint8_t, SHA_HASH_LEN>>();
    Utils::cArrayFromHex(_hex, result->data(), SHA_HASH_LEN);
    return make_shared<BLAKE3Hash>(result);
}

string BLAKE3Hash::toHex() {
    CHECK_STATE(hash);
    auto result = Utils::carray2Hex(hash->data(), SHA_HASH_LEN);
    CHECK_STATE(result != "");
    return result;
}


int BLAKE3Hash::compare(const ptr<BLAKE3Hash>& _hash2 ) {
    CHECK_ARGUMENT( _hash2 );
    CHECK_STATE(hash);

    for (size_t i = 0; i < SHA_HASH_LEN; i++) {
        if (hash->at(i) < _hash2->at(i))
            return -1;
        if (hash->at(i) > _hash2->at(i))
            return 1;
    }
    return 0;
}

BLAKE3Hash::BLAKE3Hash(const ptr<array<uint8_t, SHA_HASH_LEN>>& _hash) {
    CHECK_ARGUMENT(_hash);
    hash = _hash;
}

ptr<BLAKE3Hash> BLAKE3Hash::calculateHash(const ptr<vector<uint8_t>>& _data) {
    CHECK_ARGUMENT(_data);
    auto digest = make_shared<array<uint8_t, SHA_HASH_LEN> >();

    CryptoPP::SHA256 hashObject;

    hashObject.Update(_data->data(), _data->size());
    hashObject.Final(digest->data());

    auto hash = make_shared<BLAKE3Hash>(digest);
    return hash;
}

ptr<BLAKE3Hash> BLAKE3Hash::merkleTreeMerge(const ptr<BLAKE3Hash>& _left, const ptr<BLAKE3Hash>& _right) {
    CHECK_ARGUMENT(_left);
    CHECK_ARGUMENT(_right);

    auto concatenation = make_shared<vector<uint8_t>>();
    concatenation->reserve(2 * SHA_HASH_LEN);

    auto leftHash = _left->getHash();
    CHECK_STATE(leftHash);

    concatenation->insert(concatenation->end(), leftHash->begin(), leftHash->end());

    auto rightHash = _right->getHash();
    CHECK_STATE(rightHash);

    concatenation->insert(concatenation->end(), rightHash->begin(), rightHash->end());

    return calculateHash(concatenation);
}

ptr<array<uint8_t, SHA_HASH_LEN>> BLAKE3Hash::getHash() const {
    CHECK_STATE(hash);
    return hash;
}


