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

    @file BlockFinalizeRequestHeader.cpp
    @author Stan Kladko
    @date 2019
*/


#include "SkaleCommon.h"
#include "Log.h"
#include "crypto/BLAKE3Hash.h"
#include "exceptions/FatalError.h"
#include "exceptions/InvalidArgumentException.h"

#include "thirdparty/json.hpp"
#include "abstracttcpserver/ConnectionStatus.h"

#include "datastructures/BlockProposal.h"
#include "datastructures/CommittedBlock.h"

#include "node/Node.h"
#include "node/NodeInfo.h"
#include "chains/Schain.h"

#include "AbstractBlockRequestHeader.h"

#include "BlockFinalizeRequestHeader.h"

using namespace std;


BlockFinalizeRequestHeader::BlockFinalizeRequestHeader(Schain &_sChain, block_id _blockID,
                                                           schain_index _proposerIndex,
                                                           node_id _nodeID,
                                                           fragment_index _fragmentIndex) :
        AbstractBlockRequestHeader(_sChain.getNodeCount(), _sChain.getSchainID(), _blockID,
                Header::BLOCK_FINALIZE_REQ, _proposerIndex) {

    CHECK_ARGUMENT(_fragmentIndex > 0);

    CHECK_ARGUMENT((uint64_t ) _fragmentIndex <= _sChain.getNodeCount() - 1)

    this->fragmentIndex = _fragmentIndex;
    this->nodeID = _nodeID;


    complete = true;
}

void BlockFinalizeRequestHeader::addFields(rapidjson::Writer<rapidjson::StringBuffer> &jsonRequest) {

    AbstractBlockRequestHeader::addFields(jsonRequest);

    jsonRequest.String("fragmentIndex");
    jsonRequest.Uint64((uint64_t ) fragmentIndex);

    jsonRequest.String("nodeID");
    jsonRequest.Uint64((uint64_t ) nodeID);

}

const node_id &BlockFinalizeRequestHeader::getNodeId() const {
    return nodeID;
}


