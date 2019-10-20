//
// Created by skale on 10/20/19.
//

#include "../SkaleCommon.h"
#include "../Log.h"
#include "../crypto/SHAHash.h"
#include "ListOfHashes.h"


ptr<SHAHash> ListOfHashes::calculateTopMerkleRoot() {
    CHECK_STATE(hashCount() > 0);
    return calculateMerkleRoot(0, hashCount());
}

ptr<SHAHash> ListOfHashes::calculateMerkleRoot(uint64_t _startIndex, uint64_t _count) {
    CHECK_ARGUMENT(_startIndex < hashCount());
    CHECK_ARGUMENT(_count != 0);

    if (_count == 1)
        return getHash(_startIndex);

    uint64_t  rightHalf;


    if (_count % 2 == 0) {
        rightHalf = _count / 2;
    } else {
        rightHalf = _count / 2 + 1;
    }

    auto leftHalf = _count - rightHalf;

    auto leftHash = calculateMerkleRoot(_startIndex, leftHalf);

    ptr<SHAHash> rightHash = nullptr;

    if (rightHalf > 0) {
        rightHash = calculateMerkleRoot(_startIndex + leftHalf, leftHalf);
    }
    return SHAHash::merkleTreeMerge(leftHash, rightHash);
}

