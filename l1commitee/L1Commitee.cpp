//
// Created by stan on 23-12-2022.
//


#include "chains/Schain.h"
#include "SkaleCommon.h"
#include "Log.h"
#include "L1Commitee.h"
#include "crypto/BLAKE3Hash.h"

uint64_t L1Commitee::getCommitteeSize(Schain* _schain) {
    CHECK_ARGUMENT(_schain);
    auto nodeCount = (uint64_t) _schain->getNodeCount();

    if (nodeCount > L1_COMMITTEE_SIZE) {
        return L1_COMMITTEE_SIZE;
    } else {
        return 3 * (nodeCount / 3) + 1;
    }
}
ptr<list< uint64_t >> L1Commitee::getCommitteeForBlock(Schain* _schain, block_id _blockID,
    ptr<vector<uint8_t>> _additionalRandom ) {
    CHECK_ARGUMENT(_schain);
    CHECK_ARGUMENT( _additionalRandom );

    auto result = committeeCache.getIfExists((uint64_t) _blockID);

    auto tmp = *_additionalRandom;

    auto bid = (uint64_t) _blockID;

    auto

    std::vector<uint8_t> bufferToHash(sizeof bid);
    std::memcpy(bufferToHash.data(), &bid, sizeof bid);




    vector<::uint64_t> blockIdAsBytes(sizeof(uint64_t));
    std::memcpy(byte_vector.data(), &value, sizeof value);






    tmp.push_back(_blockID.)

    if (result.has_value()) {
        return any_cast<ptr<list<uint64_t>>>(result);
    };

    BLAKE3Hash hash == BLAKE3Hash::calculateHash()




}

uint64_t L1Commitee::getCommitteeSize(Schain* _schain) {
    CHECK_ARGUMENT(_schain);
    auto nodeCount = (uint64_t) _schain->getNodeCount();

    if (nodeCount > L1_COMMITTEE_SIZE) {
        return L1_COMMITTEE_SIZE;
    } else {
        return 3 * (nodeCount / 3) + 1;
    }
}
