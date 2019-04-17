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

    @file SHAHash.cpp
    @author Stan Kladko
    @date 2019
*/

#include "../SkaleConfig.h"
#include "../thirdparty/json.hpp"
#include "../Log.h"

#include "../headers/Header.h"
#include "../network/Utils.h"

#include "../network/Utils.h"
#include "../exceptions/InvalidArgumentException.h"

#include "SHAHash.h"

void SHAHash::print() {
    for (size_t i = 0; i < SHA3_HASH_LEN; i++) {
        cerr << to_string((*hash)[i]);
    }

}



uint8_t SHAHash::at(uint32_t _position) {
    return (*hash)[_position];
}


ptr< SHAHash > SHAHash::fromHex(ptr<string> _hex) {

    auto result =  make_shared<array<uint8_t ,SHA3_HASH_LEN>>();

    cArrayFromHex(*_hex, result->data(), SHA3_HASH_LEN);

    return make_shared< SHAHash >( result );
}





void  SHAHash::cArrayFromHex(string & _hex, uint8_t* _data, size_t len) {
    if (_hex.size()  / 2 != len) {
        BOOST_THROW_EXCEPTION(InvalidArgumentException("Misformatted string:" + _hex, __CLASS_NAME__ ));
    }

    for (size_t i = 0; i < _hex.size() / 2; i++) {
        _data[i] = Utils::char2int(_hex[2 * i]) * 16 + Utils::char2int((_hex)[2 * i + 1]);
    }

}


ptr< string > SHAHash::toHex() {
    return Utils::carray2Hex(hash->data(), SHA3_HASH_LEN);
}


int SHAHash::compare(ptr<SHAHash> hash2) {


    for (size_t i = 0; i < SHA3_HASH_LEN; i++) {


        if ((*hash)[i] < hash2->at(i))
            return -1;
        if ((*hash)[i] > hash2->at(i))
            return 1;
    }

    return 0;


}

SHAHash::SHAHash(ptr<array<uint8_t, SHA3_HASH_LEN>> _hash) {
    hash = _hash;

}
