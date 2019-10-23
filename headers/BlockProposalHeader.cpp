/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with skale-consensus.  If not, see <https://www.gnu.org/licenses/>.

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

BlockProposalHeader::BlockProposalHeader(nlohmann::json _proposalRequest, node_count _nodeCount)
        : AbstractBlockRequestHeader(_nodeCount, (schain_id) Header::getUint64(_proposalRequest, "schainID"),
                                     (block_id) Header::getUint64(_proposalRequest, "blockID"),
                                     Header::BLOCK_PROPOSAL_REQ,
                                     (schain_index) Header::getUint64(_proposalRequest, "proposerIndex")) {

    proposerNodeID = (node_id) Header::getUint64(_proposalRequest, "proposerNodeID");
    timeStamp = Header::getUint64(_proposalRequest, "timeStamp");
    timeStampMs = Header::getUint32(_proposalRequest, "timeStampMs");
    hash = Header::getString(_proposalRequest, "hash");
    signature = Header::getString(_proposalRequest, "sig");
    txCount = Header::getUint64(_proposalRequest, "txCount");
}

BlockProposalHeader::BlockProposalHeader(Schain &_sChain, ptr<BlockProposal> proposal) :
        AbstractBlockRequestHeader(_sChain.getNodeCount(), _sChain.getSchainID(), proposal->getBlockID(),
                                   Header::BLOCK_PROPOSAL_REQ,
                                   _sChain.getSchainIndex()) {


    this->proposerNodeID = _sChain.getNode()->getNodeID();
    this->txCount = (uint64_t) proposal->getTransactionCount();
    this->timeStamp = proposal->getTimeStamp();
    this->timeStampMs = proposal->getTimeStampMs();

    this->hash = proposal->getHash()->toHex();

    this->signature = proposal->getSignature();


    ASSERT(timeStamp > MODERN_TIME);

    complete = true;

}

void BlockProposalHeader::addFields(nlohmann::basic_json<> &jsonRequest) {

    AbstractBlockRequestHeader::addFields(jsonRequest);

    jsonRequest["schainID"] = (uint64_t) schainID;
    jsonRequest["proposerNodeID"] = (uint64_t) proposerNodeID;
    jsonRequest["proposerIndex"] = (uint64_t) proposerIndex;
    jsonRequest["blockID"] = (uint64_t) blockID;
    jsonRequest["txCount"] = txCount;
    ASSERT(timeStamp > MODERN_TIME);
    jsonRequest["timeStamp"] = timeStamp;
    jsonRequest["timeStampMs"] = timeStampMs;
    CHECK_STATE(hash != nullptr);
    CHECK_STATE(signature != nullptr);
    jsonRequest["hash"] = *hash;
    jsonRequest["sig"] = *signature;
}

const node_id &BlockProposalHeader::getProposerNodeId() const {
    return proposerNodeID;
}

const ptr<string> &BlockProposalHeader::getHash() const {
    return hash;
}

uint64_t BlockProposalHeader::getTxCount() const {
    return txCount;
}

uint64_t BlockProposalHeader::getTimeStamp() const {
    return timeStamp;
}

uint32_t BlockProposalHeader::getTimeStampMs() const {
    return timeStampMs;
}

ptr<string> BlockProposalHeader::getSignature() const {
    return signature;
}


