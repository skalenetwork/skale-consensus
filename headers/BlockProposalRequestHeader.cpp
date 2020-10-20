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

#include "SkaleCommon.h"
#include "Log.h"
#include "abstracttcpserver/ConnectionStatus.h"
#include "crypto/SHAHash.h"
#include "exceptions/FatalError.h"
#include "thirdparty/json.hpp"

#include "datastructures/BlockProposal.h"
#include "node/Node.h"
#include "node/NodeInfo.h"
#include "chains/Schain.h"

#include "AbstractBlockRequestHeader.h"

#include "BlockProposalRequestHeader.h"

using namespace std;

BlockProposalRequestHeader::BlockProposalRequestHeader(nlohmann::json _proposalRequest, node_count _nodeCount)
        : AbstractBlockRequestHeader(_nodeCount, (schain_id) Header::getUint64(_proposalRequest, "schainID"),
                                     (block_id) Header::getUint64(_proposalRequest, "blockID"),
                                     Header::BLOCK_PROPOSAL_REQ,
                                     (schain_index) Header::getUint64(_proposalRequest, "proposerIndex")) {

    proposerNodeID = (node_id) Header::getUint64(_proposalRequest, "proposerNodeID");
    timeStamp = Header::getUint64(_proposalRequest, "timeStamp");
    timeStampMs = Header::getUint32(_proposalRequest, "timeStampMs");
    hash = Header::getString(_proposalRequest, "hash");
    CHECK_STATE(hash);
    signature = Header::getString(_proposalRequest, "sig");
    CHECK_STATE(signature);
    txCount = Header::getUint64(_proposalRequest, "txCount");
    auto stateRootStr = Header::getString(_proposalRequest, "sr");
    CHECK_STATE(stateRootStr);
    stateRoot = u256(*stateRootStr);
    CHECK_STATE(stateRoot != 0);
}

BlockProposalRequestHeader::BlockProposalRequestHeader(Schain &_sChain, const ptr<BlockProposal> proposal) :
        AbstractBlockRequestHeader(_sChain.getNodeCount(), _sChain.getSchainID(), proposal->getBlockID(),
                                   Header::BLOCK_PROPOSAL_REQ,
                                   _sChain.getSchainIndex()) {


    proposerNodeID = _sChain.getNode()->getNodeID();
    txCount = (uint64_t) proposal->getTransactionCount();
    timeStamp = proposal->getTimeStamp();
    timeStampMs = proposal->getTimeStampMs();

    hash = proposal->getHash()->toHex();
    CHECK_STATE(hash);

    signature = proposal->getSignature();

    stateRoot = proposal->getStateRoot();

    CHECK_STATE(stateRoot != 0);
    CHECK_STATE(timeStamp > MODERN_TIME);

    complete = true;

}

void BlockProposalRequestHeader::addFields(nlohmann::basic_json<> &jsonRequest) {

    AbstractBlockRequestHeader::addFields(jsonRequest);

    jsonRequest["schainID"] = (uint64_t) schainID;
    jsonRequest["proposerNodeID"] = (uint64_t) proposerNodeID;
    jsonRequest["proposerIndex"] = (uint64_t) proposerIndex;
    jsonRequest["blockID"] = (uint64_t) blockID;
    jsonRequest["txCount"] = txCount;
    CHECK_STATE(timeStamp > MODERN_TIME);
    jsonRequest["timeStamp"] = timeStamp;
    jsonRequest["timeStampMs"] = timeStampMs;
    CHECK_STATE(hash);
    CHECK_STATE(signature);
    jsonRequest["hash"] = *hash;
    jsonRequest["sig"] = *signature;
    jsonRequest["sr"] = stateRoot.str();
}

 node_id BlockProposalRequestHeader::getProposerNodeId()  {
    return proposerNodeID;
}

 ptr<string> BlockProposalRequestHeader::getHash()  {
    CHECK_STATE(hash);
    return hash;
}

uint64_t BlockProposalRequestHeader::getTxCount()  {
    return txCount;
}

uint64_t BlockProposalRequestHeader::getTimeStamp()  {
    return timeStamp;
}

uint32_t BlockProposalRequestHeader::getTimeStampMs()  {
    return timeStampMs;
}

ptr<string> BlockProposalRequestHeader::getSignature()  {
    CHECK_STATE(signature);
    return signature;
}

 u256 BlockProposalRequestHeader::getStateRoot()  {
    return stateRoot;
}


