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

    @file ConsensusBLSPrivateKey.cpp
    @author Stan Kladko
    @date 2019
*/


#include "../Log.h"
#include "../SkaleCommon.h"
#include "../exceptions/FatalError.h"
#include "../exceptions/InvalidArgumentException.h"
#include "../network/Utils.h"
#include "../thirdparty/json.hpp"
#include "SHAHash.h"

#include "../crypto/bls_include.h"

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <iostream>


#include "BLSPrivateKeyShare.h"
#include "BLSSigShare.h"
#include "ConsensusBLSPrivateKeyShare.h"
#include "ConsensusBLSSigShare.h"


ConsensusBLSPrivateKeyShare::ConsensusBLSPrivateKeyShare( const string& k, node_count _nodeCount ) :
BLSPrivateKeyShare(k, size_t(_nodeCount), size_t(_nodeCount))
{

    if (totalSigners == 1 || totalSigners == 2)
        requiredSigners = totalSigners;
    else {
        requiredSigners = 2 * totalSigners / 3 + 1;
    }

}


ptr<ConsensusBLSSigShare>
ConsensusBLSPrivateKeyShare::sign(ptr<string> _msg, schain_id _schainId, block_id _blockId, schain_index _signerIndex,
                    node_id _signerNodeId) {


    auto blsSigShare = BLSPrivateKeyShare::sign(_msg, (size_t) _signerIndex);

    auto sigShare = make_shared<ConsensusBLSSigShare>( blsSigShare, _schainId, _blockId, _signerNodeId );

    return sigShare;
}





