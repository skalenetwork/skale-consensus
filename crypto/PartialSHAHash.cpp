//
// Created by kladko on 3/21/19.
//


#include "../SkaleConfig.h"
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

