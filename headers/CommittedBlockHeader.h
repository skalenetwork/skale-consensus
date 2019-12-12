//
// Created by kladko on 29.11.19.
//

#ifndef SKALED_COMMITTEDBLOCKHEADER_H
#define SKALED_COMMITTEDBLOCKHEADER_H


#include "BlockProposalHeader.h"

class CommittedBlockHeader : public BlockProposalHeader {
    ptr<string> thresholdSig;

public:
    CommittedBlockHeader(BlockProposal &block, const ptr<string> &thresholdSig);

    CommittedBlockHeader(nlohmann::json &json);

    const ptr<string> &getThresholdSig() const;

    void addFields(nlohmann::basic_json<> &j) override;
};


#endif //SKALED_COMMITTEDBLOCKHEADER_H
