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

    @file PartialHash.cpp
    @author Stan Kladko
    @date 2019
*/


#include "SkaleCommon.h"
#include "Log.h"
#include "thirdparty/json.hpp"
#include "PartialHash.h"
#include "network/Utils.h"


void PartialHash::print() {
    CHECK_STATE(hash);
    for (size_t i = 0; i < PARTIAL_HASH_LEN; i++) {
        cerr << to_string(hash->at(i));
    }
}

uint8_t PartialHash::at(uint32_t _position) {
    CHECK_STATE(hash);
    return hash->at(_position);
}

ptr< PartialHash > PartialHash::hex2sha(const string& _hex ) {

    CHECK_STATE(_hex != "");

    auto result = make_shared<array<uint8_t, PARTIAL_HASH_LEN>>();

    for (size_t i = 0; i < PARTIAL_HASH_LEN; i++ ) {
        result->at(i) = Utils::char2int(_hex.at(i) ) * 16 + Utils::char2int(_hex.at(i + 1));
    }

    return make_shared<PartialHash>(result );
}





int PartialHash::compare(const ptr<PartialHash>& _hash2 ) {
    CHECK_ARGUMENT(_hash2);
    CHECK_STATE(hash);
    for (size_t i = 0; i < PARTIAL_HASH_LEN; i++) {
        if (hash->at(i) < _hash2->at(i))
            return -1;
        if (hash->at(i) > _hash2->at(i))
            return 1;
    }
    return 0;
}

PartialHash::PartialHash(const ptr<array<uint8_t, PARTIAL_HASH_LEN>>& _hash) {
    CHECK_ARGUMENT(_hash);
    hash = _hash;
}



ptr< PartialHash > PartialHash::fromHex(const string& _hex) {
    CHECK_ARGUMENT(_hex != "");
    auto result = make_shared<array<uint8_t, PARTIAL_HASH_LEN>>();
    for (size_t i = 0; i < PARTIAL_HASH_LEN; i++ ) {
        result->at(i) = Utils::char2int(_hex.at(2*i) ) * 16 + Utils::char2int(_hex.at(2* i + 1));
    }
    return make_shared<PartialHash>(result);
}


string PartialHash::toHex() {
    CHECK_STATE(hash);
    return Utils::carray2Hex(hash->data(), PARTIAL_HASH_LEN);
}

