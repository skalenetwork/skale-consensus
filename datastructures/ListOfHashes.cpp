//
// Created by skale on 10/20/19.
//

#include "../SkaleCommon.h"
#include "../Log.h"
#include "../crypto/SHAHash.h"
#include "ListOfHashes.h"


ptr<SHAHash> ListOfHashes::calculateTopMerkleRoot() {
    CHECK_STATE(size() > 0);
    return calculateMerkleRoot(0, size());
}

ptr<SHAHash> ListOfHashes::calculateMerkleRoot(uint64_t _startIndex, uint64_t _count) {
    CHECK_ARGUMENT(_startIndex < size());
    CHECK_ARGUMENT(_count != 0);
    if (_count == 1)
        return getHash(_startIndex);

    auto rightHalf = count % 2;

    if (rightHalf == 0) {
        rightHalf = count / 2;
    }

    auto leftHalf = count - rightHalf;

    auto leftHash = calculateMerkleRoot(_startIndex, leftHalf);

    ptr<SHAHash> rightHash = nullptr;

    if (rightHalf > 0) {
        rightHash = calculateMerkleRoot(_startIndex + leftHalf, count - rightHalf);
    }
    return mergeHashes(leftHash, rightHash);
}

