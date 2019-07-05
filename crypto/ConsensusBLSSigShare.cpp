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

    @file ConsensusBLSSigShare.cpp
    @author Stan Kladko
    @date 2019
*/


#include "../Log.h"
#include "../SkaleCommon.h"
#include "../exceptions/FatalError.h"
#include "../exceptions/InvalidArgumentException.h"

#include "../network/Utils.h"
#include "../thirdparty/json.hpp"

#include "bls_include.h"

#include "BLSSigShare.h"
#include "ConsensusBLSSigShare.h"


ConsensusBLSSigShare::ConsensusBLSSigShare(
    ptr< BLSSigShare >& _sigShare, schain_id _schainID, block_id _blockID, node_id _signerNodeID )
    : schainId( _schainID ), blockId( _blockID ), signerNodeId( _signerNodeID ) {
    ASSERT( _sigShare != nullptr );
    blsSigShare = _sigShare;
}


node_id ConsensusBLSSigShare::getSignerNodeId() const {
    return signerNodeId;
}

block_id ConsensusBLSSigShare::getBlockId() const {
    return blockId;
}
ptr< BLSSigShare > ConsensusBLSSigShare::getBlsSigShare() const {
    ASSERT( blsSigShare );
    return blsSigShare;
}
ConsensusBLSSigShare::ConsensusBLSSigShare( ptr< string > _sigShare, schain_id _schainID,
    block_id _blockID, node_id _signerNodeID, schain_index _signerIndex )
    : schainId( _schainID ), blockId( _blockID ), signerNodeId( _signerNodeID ) {
    this->blsSigShare = make_shared< BLSSigShare >( _sigShare, ( size_t ) _signerIndex );
}
