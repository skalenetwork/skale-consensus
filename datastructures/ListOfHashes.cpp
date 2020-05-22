/*
    Copyright (C) 2019-Present SKALE Labs

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

    @file ListOfHashes.cpp
    @author Stan Kladko
    @date 2019
*/


#include "SkaleCommon.h"
#include "SkaleLog.h"

#include "crypto/SHAHash.h"

#include "ListOfHashes.h"




ptr<SHAHash> ListOfHashes::calculateTopMerkleRoot() {

    LOCK(m)

    CHECK_STATE(hashCount() > 0);

    vector<ptr<SHAHash>> hashes;
    hashes.reserve(hashCount() + 1);

    for (uint64_t i = 0; i < hashCount(); i++) {
        hashes.push_back(getHash(i));
    }

    while (hashes.size() > 1) {

        if (hashes.size() % 2 == 1)
            hashes.push_back(hashes.back());

        for (uint64_t j = 0; j < hashes.size() / 2; j++) {
            hashes[j] = SHAHash::merkleTreeMerge(hashes[2 * j], hashes[2 * j + 1]);
        }

        hashes.resize(hashes.size() / 2);
    }

    return hashes.front();

}


