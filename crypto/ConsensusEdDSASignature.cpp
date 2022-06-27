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

#include "chains/Schain.h"
#include "ConsensusEdDSASigShare.h"
#include "ConsensusEdDSASignature.h"
#include "ThresholdSignature.h"


ConsensusEdDSASignature::ConsensusEdDSASignature(
    const string& _mergedSig, schain_id _schainId, block_id _blockId, size_t _totalSigners, size_t _requiredSigners )
    : ThresholdSignature( _blockId, _totalSigners, _requiredSigners ), mergedSig(_mergedSig) {

    CHECK_ARGUMENT(!_mergedSig.empty());

    boost::char_separator< char > sep( "*" );
    boost::tokenizer tok {_mergedSig, sep};

    for ( const auto& it : tok) {
        auto share = make_shared<ConsensusEdDSASigShare>(
                                          it, _schainId, _blockId, _totalSigners);

        auto index = share->getSignerIndex();

        CHECK_STATE2(shares.count((uint64_t )index) == 0, "Duplicate shares in EdDsaThresholdSig");

        shares.emplace(index, share);

    }

    CHECK_ARGUMENT2(shares.size() == _requiredSigners, "Incorrect shares count:" +
        to_string(shares.size()));




}

string  ConsensusEdDSASignature::toString() {
    return mergedSig;
};

void ConsensusEdDSASignature::verify(
    CryptoManager& _cryptoManager,
    BLAKE3Hash& _hash) {

    try {

        for (auto & share: shares ) {
            share.second->verify(_cryptoManager, _hash,
                _cryptoManager.getSchain()->getNodeIDByIndex(share.first));
        }

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}




