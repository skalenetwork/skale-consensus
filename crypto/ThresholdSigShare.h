//
// Created by kladko on 04.10.19.
//

#ifndef SKALED_THRESHOLDSIGSHARE_H
#define SKALED_THRESHOLDSIGSHARE_H


#include "BLSSigShare.h"
#include "../cmake-build-debug/consensus_pch/SkaleCommon.h"

class ThresholdSigShare {

protected:
    schain_id schainId;
    block_id blockId;
    node_id signerNodeId;

public:
    node_id getSignerNodeId() const;

    block_id getBlockId() const;

    ThresholdSigShare(const schain_id &schainId, const block_id &blockId, const node_id &signerNodeId);
};


#endif //SKALED_THRESHOLDSIGSHARE_H
