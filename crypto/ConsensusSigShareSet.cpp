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

#include "Log.h"
#include "SkaleCommon.h"

#include "ConsensusBLSSignature.h"

#include "bls_include.h"
#include "exceptions/FatalError.h"


#include "ConsensusBLSSigShare.h"

#include "ConsensusSigShareSet.h"


using namespace std;


ConsensusSigShareSet::ConsensusSigShareSet(
    block_id _blockId, size_t _totalSigners, size_t _requiredSigners )
    : ThresholdSigShareSet( _blockId, _totalSigners, _requiredSigners ),
      blsSet( _requiredSigners, _totalSigners ) {
    totalObjects++;
}

ConsensusSigShareSet::~ConsensusSigShareSet() {
    totalObjects--;
}


ptr< ThresholdSignature > ConsensusSigShareSet::mergeSignature() {
    ptr< BLSSignature > blsSig = nullptr;

    {
        LOCK( blsSetLock );
        CHECK_STATE( blsSet.isEnough() );
        blsSig = blsSet.merge();
    }
    CHECK_STATE( blsSig );

    auto sig = make_shared< ConsensusBLSSignature >(
        blsSig, blockId, blsSig->getTotalSigners(), blsSig->getRequiredSigners() );
    return sig;
}

bool ConsensusSigShareSet::isEnough() {
    {
        LOCK( blsSetLock );
        return blsSet.isEnough();
    }
}


bool ConsensusSigShareSet::addSigShare( const ptr< ThresholdSigShare >& _sigShare ) {
    CHECK_ARGUMENT( _sigShare );
    ptr< ConsensusBLSSigShare > s = dynamic_pointer_cast< ConsensusBLSSigShare >( _sigShare );
    CHECK_STATE( s );

    {
        LOCK( blsSetLock );
        return blsSet.addSigShare( s->getBlsSigShare() );
    }
}
