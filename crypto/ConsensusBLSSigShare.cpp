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

    @file ConsensusBLSSigShare.cpp
    @author Stan Kladko
    @date 2019
*/


#include "SkaleCommon.h"
#include "Log.h"

#include "network/Utils.h"
#include "thirdparty/json.hpp"

#include "bls_include.h"

#include "BLSSigShare.h"
#include "ConsensusBLSSigShare.h"


ConsensusBLSSigShare::ConsensusBLSSigShare(ptr<BLSSigShare> _s, schain_id _schainId, block_id _blockID)
    : ThresholdSigShare(_schainId, _blockID, _s->getSignerIndex()) {
    ASSERT(_s != nullptr );
    blsSigShare = _s;
}


ptr< BLSSigShare > ConsensusBLSSigShare::getBlsSigShare() const {
    return blsSigShare;
}
ConsensusBLSSigShare::ConsensusBLSSigShare(ptr<string> _sigShare, schain_id _schainID, block_id _blockID,
                                           schain_index _signerIndex,
                                           uint64_t _totalSigners, uint64_t _requiredSigners)
    : ThresholdSigShare(_schainID, _blockID, _signerIndex) {

    try {



        this->blsSigShare = make_shared< BLSSigShare >(
            _sigShare, ( uint64_t ) _signerIndex, _requiredSigners, _totalSigners );
    }  catch ( ... ) {
        throw_with_nested( InvalidStateException( "Could not create BLSSigShare", __CLASS_NAME__ ) );
    }
}

ConsensusBLSSigShare::~ConsensusBLSSigShare() {

}

ptr<std::string> ConsensusBLSSigShare::toString() {
    try {
        return getBlsSigShare()->toString();
    }  catch ( ... ) {
        throw_with_nested( InvalidStateException( "Could not toString() sig share", __CLASS_NAME__ ) );
    }
}
