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

    @file DAProofRequestHeader.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "chains/Schain.h"
#include "crypto/SHAHash.h"
#include "crypto/ThresholdSignature.h"
#include "datastructures/BlockProposal.h"
#include "datastructures/DAProof.h"
#include "exceptions/FatalError.h"
#include "node/NodeInfo.h"
#include "thirdparty/json.hpp"

#include "AbstractBlockRequestHeader.h"

#include "SubmitDAProofRequestHeader.h"

using namespace std;

SubmitDAProofRequestHeader::SubmitDAProofRequestHeader(nlohmann::json _proposalRequest, node_count _nodeCount)
        : AbstractBlockRequestHeader(_nodeCount, (schain_id) Header::getUint64(_proposalRequest, "schainID"),
                                     (block_id) Header::getUint64(_proposalRequest, "blockID"),
                                     Header::DA_PROOF_REQ,
                                     (schain_index) Header::getUint64(_proposalRequest, "proposerIndex")) {

    proposerNodeID = (node_id) Header::getUint64(_proposalRequest, "proposerNodeID");
    thresholdSig = Header::getString(_proposalRequest, "thrSig");
    CHECK_STATE(!thresholdSig.empty())
    blockHash = Header::getString(_proposalRequest, "hash");
    CHECK_STATE(!blockHash.empty())
}

SubmitDAProofRequestHeader::SubmitDAProofRequestHeader(Schain &_sChain, const ptr<DAProof>& _proof, block_id _blockId) :
        AbstractBlockRequestHeader(_sChain.getNodeCount(), _sChain.getSchainID(), _blockId,
                                   Header::DA_PROOF_REQ,
                                   _sChain.getSchainIndex()) {




    this->proposerNodeID = _sChain.getNode()->getNodeID();

    this->thresholdSig = _proof->getThresholdSig()->toString();

    this->blockHash = _proof->getHash()->toHex();

    complete = true;

}

void SubmitDAProofRequestHeader::addFields(nlohmann::json &_jsonRequest) {

    AbstractBlockRequestHeader::addFields(_jsonRequest);

    _jsonRequest["schainID"] = (uint64_t) schainID;
    _jsonRequest["proposerNodeID"] = (uint64_t) proposerNodeID;
    _jsonRequest["proposerIndex"] = (uint64_t) proposerIndex;
    _jsonRequest["blockID"] = (uint64_t) blockID;
    CHECK_STATE(!thresholdSig.empty())
    _jsonRequest["thrSig"] = thresholdSig;
    _jsonRequest["hash"] = blockHash;
}


 node_id SubmitDAProofRequestHeader::getProposerNodeId() const  {
    return proposerNodeID;
}


string SubmitDAProofRequestHeader::getSignature() const  {
    CHECK_STATE(!thresholdSig.empty())
    return thresholdSig;
}

string SubmitDAProofRequestHeader::getBlockHash() const  {
    CHECK_STATE(!blockHash.empty())
    return blockHash;
}


