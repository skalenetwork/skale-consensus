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

#include "Log.h"
#include "SkaleCommon.h"
#include "exceptions/FatalError.h"

#include "CatchupRequestHeader.h"
#include "abstracttcpserver/ConnectionStatus.h"
#include "node/Node.h"
#include "thirdparty/json.hpp"


#include "chains/Schain.h"
#include "node/Node.h"
#include "node/NodeInfo.h"


using namespace std;

CatchupRequestHeader::CatchupRequestHeader() : Header( Header::BLOCK_CATCHUP_REQ ) {}

CatchupRequestHeader::CatchupRequestHeader( Schain& _sChain, schain_index _dstIndex )
    : CatchupRequestHeader() {
    this->schainID = _sChain.getSchainID();
    this->blockID = _sChain.getLastCommittedBlockID();
    this->nodeID = _sChain.getNode()->getNodeID();

    ASSERT( _sChain.getNode()->getNodeInfoByIndex( _dstIndex ) != nullptr );

    complete = true;
}

void CatchupRequestHeader::addFields( rapidjson::Writer< rapidjson::StringBuffer >& _j ) {
    Header::addFields( _j );

    _j.String( "schainID" );
    _j.Uint64( ( uint64_t ) schainID );

    _j.String( "blockID" );
    _j.Uint64( ( uint64_t ) blockID );

    _j.String( "nodeID" );
    _j.Uint64( ( uint64_t ) nodeID );
}

const node_id& CatchupRequestHeader::getNodeId() const {
    return nodeID;
}
