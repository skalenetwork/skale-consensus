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

    @file BLSSigShare.h
    @author Stan Kladko
    @date 2019
*/

#ifndef SKALED_BLSSIGNATURE_SHARE_H
#define SKALED_BLSSIGNATURE_SHARE_H




namespace libff {
class alt_bn128_G1;
}

class BLSSigShare {
    ptr< libff::alt_bn128_G1 > sig;
    schain_id schainId;
    block_id blockId;
    schain_index signerIndex; // converted
    node_id signerNodeId;


public:
    const ptr< libff::alt_bn128_G1 >& getSig() const;

    ptr< string > toString();


    BLSSigShare(ptr<string> _s, schain_id _schainID, block_id _blockID, schain_index _signerIndex, node_id _signerNodeID);

    BLSSigShare(ptr<libff::alt_bn128_G1> &_s, schain_id _schainId, block_id _blockID, schain_index _signerIndex,
                node_id _nodeID);

    const block_id& getBlockId() const;

    const schain_index& getSignerIndex() const;

    const node_id& getSignerNodeId() const;
};


#endif  // SKALED_BLSSignatureShare_H
