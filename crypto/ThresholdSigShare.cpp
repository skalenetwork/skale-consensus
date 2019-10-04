//
// Created by kladko on 03.10.19.
//

#include "ConsensusBLSSigShare.h"
#include "BLSSigShare.h"
#include "bls_include.h"
#include "../thirdparty/json.hpp"
#include "../network/Utils.h"
#include "../Log.h"
#include "../SkaleCommon.h"
#include "ThresholdSigShare.h"

node_id ThresholdSigShare::getSignerNodeId() const {
    return signerNodeId;
}

block_id ThresholdSigShare::getBlockId() const {
    return blockId;
}

ThresholdSigShare::ThresholdSigShare(const schain_id &_schainId, const block_id &_blockId, const node_id &_signerNodeId,
                                     schain_index _signerIndex)
        : blockId(_blockId), schainId(_schainId), signerNodeId(_signerNodeId), signerIndex(_signerIndex) {}

schain_id ThresholdSigShare::getSchainId() const {
    return schainId;
}

schain_index ThresholdSigShare::getSignerIndex() const {
    return signerIndex;
}


