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


ConsensusSigShareSet::ConsensusSigShareSet(block_id _blockId, size_t _totalSigners, size_t _requiredSigners )
    : requiredSigners(_requiredSigners), totalSigners(_totalSigners), blockId( _blockId ), blsSet(_totalSigners, _requiredSigners)  {

    totalObjects++;
}

ConsensusSigShareSet::~ConsensusSigShareSet() {
    totalObjects--;
}



ptr< ConsensusBLSSignature > ConsensusSigShareSet::mergeSignature() {
    auto blsShare = blsSet.merge();
    // BOOST_REQUIRE(obj.Verification(hash, common_signature, pk) == false);
    return make_shared<ConsensusBLSSignature>( blsShare->getSig(), blockId,
            blsShare->getTotalSigners(), blsShare->getRequiredSigners());
}

bool ConsensusSigShareSet::isEnough() {
    return blsSet.isEnough();
}



bool ConsensusSigShareSet::isEnoughMinusOne() {
    auto sigsCount = blsSet.getTotalSigSharesCount();
    return sigsCount >= requiredSigners - 1;
}


uint64_t ConsensusSigShareSet::getTotalObjects() {
    return totalObjects;
}

bool ConsensusSigShareSet::addSigShare(std::shared_ptr<ThresholdSigShare> _sigShare) {
    return blsSet.addSigShare(dynamic_pointer_cast<ConsensusBLSSigShare>(_sigShare)->getBlsSigShare());
}

