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

    @file ConsensusEdDSASigShare.h
    @author Stan Kladko
    @date 2019
*/

#ifndef SKALED_CONSENSUSEDDSASIGSHARE_H
#define SKALED_CONSENSUSEDDSASIGSHARE_H

#include "ThresholdSigShare.h"
#include "CryptoManager.h"

class ConsensusEdDSASigShare : public ThresholdSigShare {
    string sigShare;
    uint64_t timestamp;
    vector< string > tokens;

public:
    ConsensusEdDSASigShare( const string& _sigShare, schain_id _schainId, block_id _blockId,
        uint64_t _timestamp, uint64_t _totalSigners );

    string toString() override;

    void verify( CryptoManager& _cryptoManager, BLAKE3Hash& _hash, schain_index _schainIndex );
};


#endif  // SKALED_CONSENSUSEDDSASIGSHARE_H
