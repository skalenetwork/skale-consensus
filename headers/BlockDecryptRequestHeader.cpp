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

    @file BlockDecryptRequestHeader.cpp
    @author Stan Kladko
    @date 2022
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

#include "BlockDecryptRequestHeader.h"

using namespace std;


BlockDecryptRequestHeader::BlockDecryptRequestHeader(Schain &_sChain, block_id _blockID,
                                                           schain_index _proposerIndex,
                                                           node_id _nodeID,
                                                           te_share_index _shareIndex,
                                                           ptr<map<uint64_t, string>> _encryptedKeys) :
        AbstractBlockRequestHeader(_sChain.getNodeCount(), _sChain.getSchainID(), _blockID,
                Header::BLOCK_DECRYPT_REQ, _proposerIndex), encryptedKeys(_encryptedKeys) {

    CHECK_ARGUMENT(_shareIndex > 0);

    CHECK_ARGUMENT(encryptedKeys->size() > 0);

    CHECK_ARGUMENT((uint64_t ) _shareIndex <= _sChain.getNodeCount())

    this->shareIndex = _shareIndex;
    this->nodeID = _nodeID;
    complete = true;
}

void BlockDecryptRequestHeader::addFields(nlohmann::basic_json<> &jsonRequest) {

    AbstractBlockRequestHeader::addFields(jsonRequest);
    jsonRequest["shareIndex"] = (uint64_t ) shareIndex;
    jsonRequest["nodeID"] = (uint64_t ) nodeID;

    auto keyMap = nlohmann::json::object();

    for (auto&& item : *this->encryptedKeys) {
        keyMap[to_string(item.first)] = item.second;
    }

    jsonRequest["encryptedKeys"] = keyMap;
}

const node_id &BlockDecryptRequestHeader::getNodeId() const {
    return nodeID;
}


