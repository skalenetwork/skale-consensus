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

    @file SigShareSet.cpp
    @author Stan Kladko
    @date 2019
*/

#include "../SkaleCommon.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "bls_include.h"
#include "../node/ConsensusEngine.h"
#include "SHAHash.h"
#include "ConsensusBLSSignature.h"

#include "../chains/Schain.h"
#include "../pendingqueue/PendingTransactionsAgent.h"
#include "ConsensusBLSSigShare.h"

#include "BLSSigShareSet.h"
#include "ConsensusSigShareSet.h"


using namespace std;

atomic< uint64_t > ConsensusSigShareSet::totalObjects( 0 );


ConsensusSigShareSet::ConsensusSigShareSet(
    Schain* _sChain, block_id _blockId, size_t _totalSigners, size_t _requiredSigners )
    : BLSSigShareSet(_totalSigners, _requiredSigners) , sChain( _sChain ), blockId( _blockId ) {



    totalObjects++;
}

ConsensusSigShareSet::~ConsensusSigShareSet() {
    totalObjects--;
}



ptr< ConsensusBLSSignature > ConsensusSigShareSet::mergeSignature() {
    auto blsShare = merge();

    // BOOST_REQUIRE(obj.Verification(hash, common_signature, pk) == false);

    return make_shared<ConsensusBLSSignature>( blsShare->getSig(), blockId,
            blsShare->getTotalSigners(), blsShare->getRequiredSigners());
}
