//
// Created by kladko on 3/28/19.
//

#include "../SkaleConfig.h"
#include "../Log.h"
#include "../thirdparty/json.hpp"
#include "../exceptions/InvalidArgumentException.h"
#include "../crypto/SHAHash.h"
#include "../datastructures/BlockProposal.h"
#include "../chains/Schain.h"

#include "AbstractBlockRequestHeader.h"


AbstractBlockRequestHeader::AbstractBlockRequestHeader(Schain &_sChain, ptr<BlockProposal> proposal,
                                                       const char *_type, schain_index _proposerIndex) :
        Header() {

    if (!proposal) {
        BOOST_THROW_EXCEPTION(InvalidArgumentException("Null proposal", __CLASS_NAME__));
    }
    this->proposerIndex = _proposerIndex;
    this->schainID = _sChain.getSchainID();
    this->blockID = proposal->getBlockID();
    this->type = _type;

}



void AbstractBlockRequestHeader::addFields(nlohmann::basic_json<> &jsonRequest) {


    jsonRequest["type"] = type;

    jsonRequest["schainID"] = (uint64_t ) schainID;

    jsonRequest["proposerIndex"] = (uint64_t ) proposerIndex;

    jsonRequest["blockID"] = (uint64_t ) blockID;

}

