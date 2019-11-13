//
// Created by skale on 10/20/19.
//

#include "../SkaleCommon.h"
#include "../Log.h"
#include "../crypto/SHAHash.h"
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


