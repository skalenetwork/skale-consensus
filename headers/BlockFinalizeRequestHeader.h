#pragma  once

#include "AbstractBlockRequestHeader.h"



class BlockFinalizeRequestHeader : public AbstractBlockRequestHeader{



    node_id proposerNodeID;
    ptr<string> hash;

public:

    BlockFinalizeRequestHeader(Schain &_sChain, ptr<CommittedBlock> _committedBlock, schain_index _proposerIndex);



    void addFields(nlohmann::basic_json<> &jsonRequest) override;

};



