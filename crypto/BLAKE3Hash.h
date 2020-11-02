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

    @file BLAKE3Hash.h
    @author Stan Kladko
    @date 2019
*/

#ifndef CONSENSUS_BLAKE3HASH_H
#define CONSENSUS_BLAKE3HASH_H

#include "deps/BLAKE3/c/blake3.h"

#define HASH_INIT(__HASH__)     blake3_hasher __HASH__; blake3_hasher_init(& __HASH__);

#define HASH_UPDATE(__HASH__, __OBJECT__) \
blake3_hasher_update(&__HASH__, reinterpret_cast<uint8_t*>(&__OBJECT__),sizeof(__OBJECT__));

#define HASH_FINAL(__HASH__, __OBJECT__) blake3_hasher_finalize(& __HASH__, __OBJECT__, BLAKE3_OUT_LEN);



class BLAKE3Hash {

    array<uint8_t ,HASH_LEN> hash;

public:


    explicit BLAKE3Hash() {};


    void print();

    uint8_t at(uint32_t _position);

    int compare(const ptr<BLAKE3Hash>& _hash2 );

    uint8_t * data() {
        return hash.data();
    };

    const array<uint8_t ,HASH_LEN>& getHash() const;

    static ptr<BLAKE3Hash> fromHex(const string& _hex);

    string toHex();

    static ptr<BLAKE3Hash> calculateHash(const ptr<vector<uint8_t>>& _data);

    static ptr<BLAKE3Hash> merkleTreeMerge(const ptr<BLAKE3Hash>& _left, const ptr<BLAKE3Hash>& _right);

};


#endif //CONSENSUS_SHA3HASH_H
