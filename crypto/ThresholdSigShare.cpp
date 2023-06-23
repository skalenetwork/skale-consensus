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

    @file ThresholdSigShare.cpp
    @author Stan Kladko
    @date 2019
*/

#include "ThresholdSigShare.h"
#include "BLAKE3Hash.h"
#include "SkaleCommon.h"


block_id ThresholdSigShare::getBlockId() const {
    return blockId;
}

ThresholdSigShare::ThresholdSigShare(
    const schain_id& schainId, const block_id& blockId, const schain_index& _signerIndex )
    : schainId( schainId ), blockId( blockId ), signerIndex( _signerIndex ) {}

ThresholdSigShare::~ThresholdSigShare() {}

schain_index ThresholdSigShare::getSignerIndex() const {
    return signerIndex;
}


BLAKE3Hash ThresholdSigShare::computeHash() {
    auto str = this->toString();
    auto v = make_shared< vector< uint8_t > >( str.size() );
    copy( str.begin(), str.end(), v->begin() );
    return BLAKE3Hash::calculateHash( v );
}
