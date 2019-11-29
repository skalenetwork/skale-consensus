//
// Created by kladko on 29.11.19.
//

#include "../SkaleCommon.h"
#include "../Log.h"
#include "../thirdparty/json.hpp"
#include "CommittedBlockHeader.h"

CommittedBlockHeader::CommittedBlockHeader(BlockProposal &block, const ptr<string> &thresholdSig) : BlockProposalHeader(
        block), thresholdSig(thresholdSig) {
    CHECK_ARGUMENT(thresholdSig != nullptr);
}

CommittedBlockHeader::CommittedBlockHeader(nlohmann::json &json) : BlockProposalHeader(json) {
    thresholdSig = Header::getString(json, "sig");
    CHECK_STATE(thresholdSig != nullptr);
}

const ptr<string> &CommittedBlockHeader::getThresholdSig() const {
    return thresholdSig;
}




