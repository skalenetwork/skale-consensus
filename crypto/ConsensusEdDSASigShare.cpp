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


#include <boost/tokenizer.hpp>


#include "SkaleCommon.h"
#include "Log.h"

#include "network/Utils.h"
#include "thirdparty/json.hpp"


#include "ConsensusEdDSASigShare.h"

ConsensusEdDSASigShare::ConsensusEdDSASigShare(const string& _sigShare, schain_id _schainID, block_id _blockID,
                                           schain_index _signerIndex,
                                           uint64_t, uint64_t)
    : ThresholdSigShare(_schainID, _blockID, _signerIndex), edDSASigShare(_sigShare) {

    CHECK_ARGUMENT(!_sigShare.empty());

    CHECK_STATE(_sigShare.find(";") != string::npos)
    this->edDSASigShare = _sigShare;
}


string ConsensusEdDSASigShare::toString() {
    return edDSASigShare;
}

void ConsensusEdDSASigShare::verify(
    CryptoManager& _cryptoManager, schain_index _signerIndex,
    ptr< BLAKE3Hash >& _hash, node_id _nodeId) {

    boost::char_separator< char > sep( ";" );
    boost::tokenizer tok {edDSASigShare, sep};

    vector<string> tokens;

    for ( const auto& it : tok) {
        tokens.push_back((it));
    }

    if (tokens.size() != 4) {
        BOOST_THROW_EXCEPTION(InvalidStateException(string("Incorrect ConsensusEdDSASigShare:") +
                                                    "tokens.size() ! = 4" + edDSASigShare,
                                                    __CLASS_NAME__));
    }

    if (to_string((uint64_t)_signerIndex) != tokens.at(0)) {
        BOOST_THROW_EXCEPTION(InvalidStateException(string("Incorrect ConsensusEdDSASigShare:") +
                                                    "SignerIndex must be " +
                                                          to_string(_signerIndex) +
                                                          ":" + edDSASigShare,
                                                    __CLASS_NAME__));
    }

    CHECK_STATE(_cryptoManager.sessionVerifySigAndKey(_hash, tokens.at(1), tokens.at(2),
        tokens.at(3), blockId, _nodeId));

}
