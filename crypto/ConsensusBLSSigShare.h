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

    @file ConsensusBLSSigShare.h
    @author Stan Kladko
    @date 2019
*/

#ifndef SKALED_CONSENSUSBLSSIGSHARE_H
#define SKALED_CONSENSUSBLSSIGSHARE_H


#include "BLSSigShare.h"

namespace libff {
class alt_bn128_G1;
}

class ConsensusBLSSigShare  {


    ptr<BLSSigShare> blsSigShare;

    schain_id schainId;
    block_id blockId;
    node_id signerNodeId;

public:




    ConsensusBLSSigShare(ptr<BLSSigShare> &_s, schain_id _schainId, block_id _blockID, node_id _signerNodeID);


    ConsensusBLSSigShare(ptr<string> _sigShare, schain_id _schainID, block_id _blockID, node_id _signerNodeID,
        schain_index _signerIndex);


    block_id getBlockId() const;
    node_id getSignerNodeId() const;
    ptr< BLSSigShare > getBlsSigShare() const;
};


#endif  // SKALED_CONSENSUSBLSSIGSHARE_H
