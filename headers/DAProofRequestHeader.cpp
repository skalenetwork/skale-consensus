/*
    Copyright (C) 2019 SKALE Labs

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
    @date 2019
*/


#include "../SkaleCommon.h"
#include "../crypto/SHAHash.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../exceptions/InvalidArgumentException.h"

#include "../thirdparty/json.hpp"
#include "../abstracttcpserver/ConnectionStatus.h"

#include "../datastructures/BlockProposal.h"
#include "../datastructures/CommittedBlock.h"

#include "../node/Node.h"
#include "../node/NodeInfo.h"
#include "../chains/Schain.h"

#include "AbstractBlockRequestHeader.h"

#include "DAProofRequestHeader.h"

using namespace std;


DAProofRequestHeader::DAProofRequestHeader(Schain &_sChain, block_id _blockID, schain_index _proposerIndex,
                                           ptr<string> _sig) :
        AbstractBlockRequestHeader(_sChain.getNodeCount(), _sChain.getSchainID(), _blockID,
                                   Header::DA_PROOF_REQ, _proposerIndex), thresholdSig(_sig) {

    CHECK_ARGUMENT(_sig != nullptr);

    complete = true;
}

void DAProofRequestHeader::addFields(nlohmann::basic_json<> &jsonRequest) {

    AbstractBlockRequestHeader::addFields(jsonRequest);

    jsonRequest["blsSig"] = *thresholdSig;

}


DAProofRequestHeader::DAProofRequestHeader(nlohmann::json _proposalRequest, node_count _nodeCount) :
        AbstractBlockRequestHeader(_nodeCount, (schain_id) Header::getUint64(_proposalRequest, "schainID"),
                                   (block_id) Header::getUint64(_proposalRequest, "blockID"),
                                   Header::DA_PROOF_REQ,
                                   (schain_index) Header::getUint64(_proposalRequest, "proposerIndex")) {


    thresholdSig = Header::getString(_proposalRequest, "thrSig");


}