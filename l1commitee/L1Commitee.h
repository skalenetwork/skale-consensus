//
// Created by stan on 23-12-2022.
//

#ifndef SKALED_L1COMMITEE_H
#define SKALED_L1COMMITEE_H

#include "thirdparty/lrucache.hpp"

class Schain;
class BLAKE3Hash;

const ::uint64_t L1_COMMITTEE_SIZE = 64;

class L1Commitee {
   cache::lru_cache<uint64_t, ptr<list<uint64_t>>> committeeCache;


public:
    ptr<list<uint64_t>> getCommitteeForBlock(block_id _blockID, ptr<BLAKE3Hash> _blockHash);

    static uint64_t getCommitteeSize(Schain* _schain);
};



#endif  // SKALED_L1COMMITEE_H
