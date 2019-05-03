/*
    Copyright (C) 2019 SKALE Labs

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

    @file BlockFinalizeRequestHeader.cpp
    @author Stan Kladko
    @date 2019
*/


#include "../SkaleConfig.h"
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

#include "BlockFinalizeRequestHeader.h"

using namespace std;


BlockFinalizeRequestHeader::BlockFinalizeRequestHeader(Schain &_sChain, ptr<CommittedBlock> proposal,
        schain_index _proposerIndex) :
        AbstractBlockRequestHeader(_sChain, static_pointer_cast<BlockProposal>(proposal),
                Header::BLOCK_FINALIZE_REQ, _proposerIndex) {

    if (!proposal) {
        BOOST_THROW_EXCEPTION(InvalidArgumentException("Null proposal", __CLASS_NAME__));
    }

    if (proposal) {
        this->hash = proposal->getHash()->toHex();
    }
    this->proposerNodeID = _sChain.getNode()->getNodeID();
    complete = true;

}

void BlockFinalizeRequestHeader::addFields(nlohmann::basic_json<> &jsonRequest) {

    AbstractBlockRequestHeader::addFields(jsonRequest);

    if (hash)
        jsonRequest["hash"] = *hash;

}


