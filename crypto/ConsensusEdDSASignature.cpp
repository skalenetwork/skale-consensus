/*
    Copyright (C) 2020 SKALE Labs

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
    @date 2020
*/

#include <boost/tokenizer.hpp>

#include "Log.h"
#include "SkaleCommon.h"
#include "network/Utils.h"
#include "thirdparty/json.hpp"

#include "ConsensusEdDSASignature.h"
#include "ThresholdSignature.h"


ConsensusEdDSASignature::ConsensusEdDSASignature(
    const string& _mergedSig, block_id _blockID, size_t _totalSigners, size_t _requiredSigners )
    : ThresholdSignature( _blockID, _totalSigners, _requiredSigners ), mergedSig(_mergedSig) {

    CHECK_ARGUMENT(!_mergedSig.empty());

    boost::char_separator< char > sep( "*" );
    boost::tokenizer tok {_mergedSig, sep};

    for ( const auto& it : tok) {
        shares.push_back((it));
    }

    assert(shares.size() == _requiredSigners);
    CHECK_ARGUMENT(shares.size() == _requiredSigners);


}

string  ConsensusEdDSASignature::toString() {
    return mergedSig;
};




