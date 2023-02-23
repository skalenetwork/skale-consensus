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

    @file ConsensusEdDSASignature.h
    @author Stan Kladko
    @date 2019
*/

#ifndef SKALED_CONSENSUSEDDSASIGNATURE_H
#define SKALED_CONSENSUSEDDSASIGNATURE_H

#include "ThresholdSignature.h"

class ConsensusEdDSASigShareSet;
class CryptoManager;

class ConsensusEdDSASignature : public ThresholdSignature {

    string mergedSig;

    map<uint64_t, ptr<ConsensusEdDSASigShare>> shares;

    uint64_t timestamp;

public:

    ConsensusEdDSASignature(
        const string& _sig, schain_id _schainId, block_id _blockID, uint64_t timestamp, size_t _totalSigners, size_t _requiredSigners );

    string toString() override;

    uint64_t getRandom() override {
        assert(false); // not implemented
    }

    void verify( CryptoManager& _cryptoManager, BLAKE3Hash& _hash);
};

#endif  // SKALED_CONSENSUSEDDSASIGNATURE_H
