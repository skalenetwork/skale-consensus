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

    @file ConsensusEdDSASigShare.cpp
    @author Stan Kladko
    @date 2019
*/





#include "SkaleCommon.h"
#include "Log.h"

#include "network/Utils.h"
#include "thirdparty/json.hpp"


#include "ConsensusEdDSASigShare.h"

ConsensusEdDSASigShare::ConsensusEdDSASigShare(const string& _sigShare, schain_id _schainID, block_id _blockID,
    uint64_t _timestamp, uint64_t _totalSigners)
    : ThresholdSigShare(_schainID, _blockID, 0), sigShare(_sigShare), timestamp(_timestamp) {



    boost::char_separator< char > sep( ";" );
    boost::tokenizer tok {sigShare, sep};

    for ( const auto& it : tok) {
        tokens.push_back((it));
    }

    if (tokens.size() != 4) {
        BOOST_THROW_EXCEPTION(InvalidStateException(string("Incorrect ConsensusEdDSASigShare:") +
                                                          "tokens.size() ! = 4" + sigShare,
            __CLASS_NAME__));
    }

    signerIndex = boost::lexical_cast<uint64_t >(tokens.at(0));

    CHECK_STATE(signerIndex > 0);
    CHECK_STATE(signerIndex <= _totalSigners);

}


string ConsensusEdDSASigShare::toString() {
    return sigShare;
}

void ConsensusEdDSASigShare::verify(
    CryptoManager& _cryptoManager,
    BLAKE3Hash& _hash, schain_index _schainIndex) {


    try {
        // EdDSA sig shares are always verified using the current set of ecdsa keys
        _cryptoManager.verifySessionSigAndKey(_hash, tokens.at(1), tokens.at(2),
                                              tokens.at(3), blockId, _cryptoManager.getHistoricNodeIDByIndex(uint64_t(_schainIndex), timestamp), timestamp);
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}
