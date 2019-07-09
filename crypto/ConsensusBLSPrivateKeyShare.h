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

    @file ConsensusBLSPrivateKey.h
    @author Stan Kladko
    @date 2019
*/

#ifndef SKALED_CONSENSUSBLSPRIVATEKEYSHARE_H
#define SKALED_CONSENSUSBLSPRIVATEKEYSHARE_H 1

#include "BLSPrivateKeyShare.h"
#include "ConsensusBLSSigShare.h"

class ConsensusBLSPrivateKeyShare : public BLSPrivateKeyShare {
public:
    ConsensusBLSPrivateKeyShare( const string& k, node_count _nodeCount );

    ptr<ConsensusBLSSigShare> sign(ptr<string> _msg, schain_id _schainId, block_id _blockId, schain_index _signerIndex,
                          node_id _signerNodeId);

    ptr< string > convertSigToString( const libff::alt_bn128_G1& signature ) const;
};


#endif


