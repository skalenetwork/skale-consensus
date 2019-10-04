//
// Created by kladko on 04.10.19.
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

ThresholdSigShare::ThresholdSigShare(const schain_id &schainId, const block_id &blockId, const node_id &signerNodeId)
        : schainId(schainId), blockId(blockId), signerNodeId(signerNodeId) {}
