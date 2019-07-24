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

    @file CatchupRequestHeader.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../SkaleCommon.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"

#include "../thirdparty/json.hpp"
#include "../node/Node.h"
#include "../abstracttcpserver/ConnectionStatus.h"
#include "CatchupRequestHeader.h"




#include "../node/Node.h"
#include "../node/NodeInfo.h"
#include "../chains/Schain.h"



using namespace std;

CatchupRequestHeader::CatchupRequestHeader() : Header(Header::CATCHUP_REQ) {
}

CatchupRequestHeader::CatchupRequestHeader(Schain &_sChain, schain_index _dstIndex) :
        CatchupRequestHeader() {


    this->srcNodeID = _sChain.getNode()->getNodeID();
    this->srcSchainIndex = _sChain.getSchainIndex();
    this->dstNodeID = _sChain.getNode()->getNodeInfoByIndex(_dstIndex)->getNodeID();
    this->schainID = _sChain.getSchainID();
    this->blockID = _sChain.getLastCommittedBlockID();

    ASSERT(_sChain.getNode()->getNodeInfoByIndex(_dstIndex) != nullptr);

    complete = true;

}

void CatchupRequestHeader::addFields(nlohmann::basic_json<> &j) {

    j["schainID"] = (uint64_t ) schainID;

    j["srcNodeID"] = (uint64_t ) srcNodeID;

    j["dstNodeID"] = (uint64_t )  dstNodeID;

    j["srcSchainIndex"] = (uint64_t ) srcSchainIndex;

    j["blockID"] = (uint64_t ) blockID;

}


