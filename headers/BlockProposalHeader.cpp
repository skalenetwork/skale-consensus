//
// Created by stan on 18.03.18.
//


#include "../SkaleConfig.h"
#include "../crypto/SHAHash.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../thirdparty/json.hpp"
#include "../abstracttcpserver/ConnectionStatus.h"

#include "../datastructures/BlockProposal.h"
#include "../node/Node.h"
#include "../node/NodeInfo.h"
#include "../chains/Schain.h"

#include "AbstractBlockRequestHeader.h"

#include "BlockProposalHeader.h"

using namespace std;


BlockProposalHeader::BlockProposalHeader(Schain &_sChain, ptr<BlockProposal> proposal) :
        AbstractBlockRequestHeader(_sChain, proposal, PROPOSAL_REQUEST_TYPE, _sChain.getSchainIndex()) {


    this->proposerNodeID = _sChain.getNode()->getNodeID();
    this->partialHashesCount = (uint64_t) proposal->getTransactionsCount();
    this->timeStamp = proposal->getTimeStamp();
    this->hash = proposal->getHash()->toHex();



    ASSERT(timeStamp > MODERN_TIME);

    complete = true;

}

void BlockProposalHeader::addFields(nlohmann::basic_json<> &jsonRequest) {

    AbstractBlockRequestHeader::addFields(jsonRequest);

    jsonRequest["schainID"] = (uint64_t ) schainID;

    jsonRequest["proposerNodeID"] = (uint64_t ) proposerNodeID;

    jsonRequest["proposerIndex"] = (uint64_t ) proposerIndex;

    jsonRequest["blockID"] = (uint64_t ) blockID;

    jsonRequest["partialHashesCount"] = partialHashesCount;

    ASSERT(timeStamp > MODERN_TIME);

    jsonRequest["timeStamp"] = timeStamp;

    jsonRequest["hash"] = *hash;

}


