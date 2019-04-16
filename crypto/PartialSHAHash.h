//
// Created by kladko on 3/21/19.
//

#ifndef CONSENSUS_PARTIALSHA3HASH_H
#define CONSENSUS_PARTIALSHA3HASH_H


#include "../SkaleConfig.h"

class PartialSHAHash {

    ptr<array<uint8_t ,PARTIAL_SHA_HASH_LEN>> hash;

public:

    explicit PartialSHAHash(ptr<array<uint8_t, PARTIAL_SHA_HASH_LEN>> _hash);

    static ptr< PartialSHAHash >  hex2sha( ptr< string > _hex );

    void print();

    uint8_t at(uint32_t _position);

    int compare(ptr<PartialSHAHash> hash);

    uint8_t * data() {
        return hash->data();
    };

    ptr<PartialSHAHash> fromHex(ptr<string> _hex);

    ptr< string > toHex();


};


#endif
