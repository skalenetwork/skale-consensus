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

#include "ConsensusEdDSASigShare.h"
#include "ConsensusEdDSASigShareSet.h"
#include "ConsensusEdDSASignature.h"
#include "exceptions/FatalError.h"


using namespace std;


ConsensusEdDSASigShareSet::ConsensusEdDSASigShareSet(
    block_id _blockId, size_t _totalSigners, size_t _requiredSigners )
    : ThresholdSigShareSet( _blockId, _totalSigners, _requiredSigners ) {
    totalObjects++;
}

ConsensusEdDSASigShareSet::~ConsensusEdDSASigShareSet() {
    totalObjects--;
}


ptr< ThresholdSignature > ConsensusEdDSASigShareSet::mergeSignature() {
    CHECK_STATE( isEnough() );


    string mergedSig;

    {
        LOCK( m );
        for ( auto&& entry : edDSASet ) {
            mergedSig.append( entry.second );
            mergedSig.append( "*" );
        }
    }
    mergedSig.pop_back();

    CHECK_STATE( !mergedSig.empty() );


    auto sig =
        make_shared< ConsensusEdDSASignature >( mergedSig, blockId, totalSigners, requiredSigners );
    return sig;
}

bool ConsensusEdDSASigShareSet::isEnough() {
        LOCK( m );
        return edDSASet.size() >= requiredSigners;
}


bool ConsensusEdDSASigShareSet::addSigShare( const ptr< ThresholdSigShare >& _sigShare ) {
    CHECK_ARGUMENT( _sigShare );
    ptr< ConsensusEdDSASigShare > s = dynamic_pointer_cast< ConsensusEdDSASigShare >( _sigShare );
    CHECK_STATE( s );
    uint64_t index = ( uint64_t ) s->getSignerIndex();

    LOCK(m) {
        if ( edDSASet.count( index ) > 0 )
            return false;
        edDSASet.emplace(index, s->toString());
    }

    return true;
}
