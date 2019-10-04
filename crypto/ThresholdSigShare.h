//
// Created by kladko on 03.10.19.
//

#ifndef SKALED_THRESHOLDSIGSHARE_H
#define SKALED_THRESHOLDSIGSHARE_H


#include "BLSSigShare.h"
#include "../cmake-build-debug/consensus_pch/SkaleCommon.h"

class ThresholdSigShare {

protected:
    block_id blockId;
    schain_id schainId;
    node_id signerNodeId;
    schain_index signerIndex;


protected:
    ThresholdSigShare(const schain_id &_schainId, const block_id &_blockId, const node_id &_signerNodeId,
                      schain_index _signerIndex);

public:

    schain_id getSchainId() const;

    node_id getSignerNodeId() const;

    block_id getBlockId() const;

    schain_index getSignerIndex() const;

    virtual std::shared_ptr<std::string> toString() = 0;
};


#endif //SKALED_THRESHOLDSIGSHARE_H
