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

    @file ConsensusEdDSASignature.cpp
    @author Stan Kladko
    @date 2019
*/


#include "Log.h"
#include "SkaleCommon.h"
#include "network/Utils.h"
#include "thirdparty/json.hpp"

#include "ConsensusEdDSASignature.h"
#include "ThresholdSignature.h"



ConsensusEdDSASignature::ConsensusEdDSASignature(
    const string& _sig, block_id _blockID, size_t _totalSigners, size_t _requiredSigners )
    : ThresholdSignature( _blockID, _totalSigners, _requiredSigners ) {

    CHECK_ARGUMENT( _sig != "");


    try {
        blsSig = make_shared< EdDSASignature >( make_shared<string>(_sig), _requiredSigners, _totalSigners );
    } catch ( ... ) {
        throw_with_nested(
            InvalidStateException( "Could not create EdDSASignature from string", __CLASS_NAME__ ) );
    }
}


static string dummy_string( "" );

ConsensusEdDSASignature::ConsensusEdDSASignature(
    string& _mergedSig, block_id _blockID, size_t _totalSigners, size_t _requiredSigners )
    : ThresholdSignature( _blockID, _totalSigners, _requiredSigners ), mergedSig(_mergedSig) {

    CHECK_ARGUMENT(!_mergedSig.empty());
}

string  ConsensusEdDSASignature::toString() {
    return mergedSig;
};




