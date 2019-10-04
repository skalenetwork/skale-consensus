//
// Created by kladko on 04.10.19.
//

#ifndef SKALED_THRESHOLDSIGSHARESET_H
#define SKALED_THRESHOLDSIGSHARESET_H


#include "../datastructures/DataStructure.h"
#include "BLSSigShareSet.h"
#include "../cmake-build-debug/consensus_pch/SkaleCommon.h"

class ThresholdSigShareSet {

protected:
    block_id blockId;
    uint64_t requiredSigners;
    static atomic<uint64_t>  totalObjects;
    uint64_t totalSigners;
public:
    ThresholdSigShareSet(const block_id &blockId, uint64_t requiredSigners, uint64_t totalSigners);

    bool isEnough();

    bool isEnoughMinusOne();

    static uint64_t getTotalObjects();
};


#endif //SKALED_THRESHOLDSIGSHARESET_H
