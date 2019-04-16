#pragma  once

#include "AbstractBlockRequestHeader.h"



class BlockProposalHeader : public AbstractBlockRequestHeader{



    node_id proposerNodeID;
    ptr<string> hash;

    uint64_t partialHashesCount;
    uint64_t  timeStamp = 0;

public:

    BlockProposalHeader(Schain &_sChain, ptr<BlockProposal> proposal);



    void addFields(nlohmann::basic_json<> &jsonRequest) override;

};



