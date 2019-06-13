/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skale-consensus.  If not, see <http://www.gnu.org/licenses/>.

    @file BlockProposalHeader.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../SkaleCommon.h"
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
        AbstractBlockRequestHeader(_sChain, proposal, Header::BLOCK_PROPOSAL_REQ, _sChain.getSchainIndex1() -1 ) { // XXXX


    this->proposerNodeID = _sChain.getNode()->getNodeID();
    this->partialHashesCount = (uint64_t) proposal->getTransactionsCount();
    this->timeStamp = proposal->getTimeStamp();
    this->timeStampMs = proposal->getTimeStampMs();

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

    jsonRequest["timeStampMs"] = timeStampMs;

    jsonRequest["hash"] = *hash;

}


