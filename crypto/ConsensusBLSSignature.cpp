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

    @file ConsensusBLSSignature.cpp
    @author Stan Kladko
    @date 2019
*/


#include "Log.h"
#include "SkaleCommon.h"
#include "network/Utils.h"
#include "thirdparty/json.hpp"

#include "ConsensusBLSSignature.h"
#include "ThresholdSignature.h"
#include "libBLS/bls/BLSSignature.h"


ConsensusBLSSignature::ConsensusBLSSignature(
    const string& _sig, block_id _blockID, size_t _totalSigners, size_t _requiredSigners )
    : ThresholdSignature( _blockID, _totalSigners, _requiredSigners ) {
    CHECK_ARGUMENT( !_sig.empty() );

    try {
        blsSig = make_shared< BLSSignature >(
            make_shared< string >( _sig ), _requiredSigners, _totalSigners );
    } catch ( ... ) {
        throw_with_nested(
            InvalidStateException( "Could not create BLSSignature from string", __CLASS_NAME__ ) );
    }
}


static string dummy_string( "" );

ConsensusBLSSignature::ConsensusBLSSignature( const ptr< BLSSignature >& _blsSig, block_id _blockID,
    size_t _totalSigners, size_t _requiredSigners )
    : ThresholdSignature( _blockID, _totalSigners, _requiredSigners ), blsSig(_blsSig) {
    CHECK_ARGUMENT( _blsSig );
}

string ConsensusBLSSignature::toString() {
    CHECK_STATE( blsSig );
    try {
        return *blsSig->toString();
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( "Could not toString() sig", __CLASS_NAME__ ) );
    }
};

uint64_t ConsensusBLSSignature::getRandom() {
    try {
        CHECK_STATE( blsSig );
        auto sig = blsSig->getSig();
        CHECK_STATE( sig );
        sig->to_affine_coordinates();
        auto result = sig->X.as_ulong() + sig->Y.as_ulong();
        return result;
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( "Could not getRandom()", __CLASS_NAME__ ) );
    }
}

ptr< BLSSignature > ConsensusBLSSignature::getBlsSig() const {
    CHECK_STATE( blsSig );
    return blsSig;
}
