/*
    Copyright (C) 2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skale-consensus.  If not, see <http://www.gnu.org/licenses/>.

    @file PartialSHAHash.cpp
    @author Stan Kladko
    @date 2019
*/


#include "../SkaleCommon.h"
#include "../thirdparty/json.hpp"
#include "../headers/Header.h"
#include "PartialSHAHash.h"
#include "../network/Utils.h"


void PartialSHAHash::print() {
    for (size_t i = 0; i < PARTIAL_SHA_HASH_LEN; i++) {
        cerr << to_string((*hash)[i]);
    }

}

uint8_t PartialSHAHash::at(uint32_t _position) {
    return (*hash)[_position];
}

ptr< PartialSHAHash > PartialSHAHash::hex2sha( ptr< string > _hex ) {

    auto result = make_shared<array<uint8_t, PARTIAL_SHA_HASH_LEN>>();

    for ( size_t i = 0; i < PARTIAL_SHA_HASH_LEN; i++ ) {
        (*result)[i] = Utils::char2int( ( *_hex )[i] ) * 16 + Utils::char2int( ( *_hex )[i + 1] );
    }

    return make_shared< PartialSHAHash >( result );
}





int PartialSHAHash::compare(ptr<PartialSHAHash> hash2) {
    for (size_t i = 0; i < PARTIAL_SHA_HASH_LEN; i++) {
        if ((*hash)[i] < hash2->at(i))
            return -1;
        if ((*hash)[i] > hash2->at(i))
            return 1;
    }
    return 0;


}

PartialSHAHash::PartialSHAHash(ptr<array<uint8_t, PARTIAL_SHA_HASH_LEN>> _hash) {
    hash = _hash;

}



ptr< PartialSHAHash > PartialSHAHash::fromHex(ptr<string> _hex) {

    auto result = make_shared<array<uint8_t, PARTIAL_SHA_HASH_LEN>>();

    for ( size_t i = 0; i < PARTIAL_SHA_HASH_LEN; i++ ) {
        (*result)[i] = Utils::char2int( ( *_hex )[2*i] ) * 16 + Utils::char2int( ( *_hex )[2* i + 1] );
    }

    return make_shared< PartialSHAHash >( result );
}


ptr< string > PartialSHAHash::toHex() {
    return Utils::carray2Hex(hash->data(), PARTIAL_SHA_HASH_LEN);
}

