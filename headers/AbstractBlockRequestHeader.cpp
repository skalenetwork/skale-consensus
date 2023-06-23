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

    @file AbstractBlockRequestHeader.cpp
    @author Stan Kladko
    @date 2019
*/


#include "SkaleCommon.h"
#include "Log.h"
#include "chains/Schain.h"
#include "crypto/BLAKE3Hash.h"
#include "datastructures/BlockProposal.h"
#include "exceptions/InvalidArgumentException.h"
#include "thirdparty/json.hpp"

#include "AbstractBlockRequestHeader.h"


AbstractBlockRequestHeader::AbstractBlockRequestHeader( node_count _nodeCount, schain_id _schainId,
    block_id _blockId, const char* _type, schain_index _proposerIndex )
    : Header( _type ), proposerIndex( _proposerIndex ), blockID( _blockId ) {
    CHECK_ARGUMENT( ( uint64_t ) proposerIndex <= ( uint64_t ) _nodeCount );

    CHECK_ARGUMENT( proposerIndex > 0 );

    this->schainID = _schainId;
}


void AbstractBlockRequestHeader::addFields( nlohmann::basic_json<>& jsonRequest ) {
    Header::addFields( jsonRequest );

    jsonRequest["schainID"] = ( uint64_t ) schainID;

    jsonRequest["proposerIndex"] = ( uint64_t ) proposerIndex;

    jsonRequest["blockID"] = ( uint64_t ) blockID;
}

const schain_id& AbstractBlockRequestHeader::getSchainId() const {
    return schainID;
}

const schain_index& AbstractBlockRequestHeader::getProposerIndex() const {
    return proposerIndex;
}

const block_id& AbstractBlockRequestHeader::getBlockId() const {
    return blockID;
}
