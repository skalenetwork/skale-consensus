//
// Created by kladko on 3/21/19.
//

#ifndef CONSENSUS_SHA3HASH_H
#define CONSENSUS_SHA3HASH_H


#include "../SkaleConfig.h"

class SHAHash {

    ptr<array<uint8_t ,SHA3_HASH_LEN>> hash;

public:

    explicit SHAHash(ptr<array<uint8_t, SHA3_HASH_LEN>> _hash);


    void print();

    uint8_t at(uint32_t _position);

    int compare(ptr<SHAHash> hash);

    uint8_t * data() {
        return hash->data();
    };


    static ptr<SHAHash> fromHex(ptr<string> _hex);

    static void cArrayFromHex(string& _str, uint8_t* data, size_t len);

    ptr< string > toHex();
};


#endif //CONSENSUS_SHA3HASH_H
