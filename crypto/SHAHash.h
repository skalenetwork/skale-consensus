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

    @file SHAHash.h
    @author Stan Kladko
    @date 2019
*/

#ifndef CONSENSUS_SHAHASH_H
#define CONSENSUS_SHAHASH_H


#define SHA3_UPDATE(__HASH__, __OBJECT__) __HASH__.Update(reinterpret_cast < uint8_t * > ( &__OBJECT__), sizeof(__OBJECT__))

class SHAHash {

    ptr<array<uint8_t ,SHA_HASH_LEN>> hash;

public:

    explicit SHAHash(ptr<array<uint8_t, SHA_HASH_LEN>> _hash);


    void print();

    uint8_t at(uint32_t _position);

    int compare(ptr<SHAHash> hash);

    uint8_t * data() {
        return hash->data();
    };

    ptr<array<uint8_t ,SHA_HASH_LEN>> getHash() const;

    static ptr<SHAHash> fromHex(ptr<string> _hex);

    ptr< string > toHex();

    static ptr<SHAHash> calculateHash(ptr<vector<uint8_t>> _data);

    static ptr<SHAHash> merkleTreeMerge(ptr<SHAHash> _left, ptr<SHAHash> _right);

};


#endif //CONSENSUS_SHA3HASH_H
