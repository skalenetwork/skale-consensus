//
// Created by kladko on 3/28/19.
//

#ifndef CONSENSUS_ABSTRACTBLOCKREQUESTHEADER_H
#define CONSENSUS_ABSTRACTBLOCKREQUESTHEADER_H



#include "Header.h"

class NodeInfo;
class BlockProposal;
class Schain;


#define PROPOSAL_REQUEST_TYPE "PROPOSAL"
#define FINALIZE_REQUEST_TYPE "FINALIZE"




class AbstractBlockRequestHeader : public Header {

protected:

    schain_id schainID;
    schain_index proposerIndex;
    block_id blockID;
    const char* type;

    void addFields(nlohmann::basic_json<> &jsonRequest) override;

    AbstractBlockRequestHeader(Schain &_sChain, ptr<BlockProposal> proposal,
                               const char *_type, schain_index _proposerIndex);

    virtual ~AbstractBlockRequestHeader(){};

public:


};



#endif //CONSENSUS_ABSTRACTBLOCKREQUESTHEADER_H
