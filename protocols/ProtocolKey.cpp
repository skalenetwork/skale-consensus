/*
    Copyright (C) 2018-2019 SKALE Labs

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

    @file ProtocolKey.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"
#include "ProtocolKey.h"

ProtocolKey::ProtocolKey( const ProtocolKey& key )
    : blockID( key.blockID ), blockProposerIndex( key.blockProposerIndex ) {
    CHECK_STATE( ( uint64_t ) blockID > 0 );
}

block_id ProtocolKey::getBlockID() const {
    CHECK_STATE( ( uint64_t ) blockID > 0 );
    return blockID;
}

schain_index ProtocolKey::getBlockProposerIndex() const {
    return blockProposerIndex;
}

ProtocolKey::ProtocolKey( block_id _blockId, schain_index _blockProposerIndex )
    : blockID( _blockId ), blockProposerIndex( _blockProposerIndex ) {
    CHECK_STATE( ( uint64_t ) blockID > 0 );
}

bool operator<( const ProtocolKey& l, const ProtocolKey& r ) {
    if ( ( uint64_t ) l.getBlockID() != ( uint64_t ) r.getBlockID() ) {
        return ( uint64_t ) l.getBlockID() < ( uint64_t ) r.getBlockID();
    }

    return ( uint64_t ) l.getBlockProposerIndex() < ( uint64_t ) r.getBlockProposerIndex();
}
