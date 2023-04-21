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

    @file BlockProposalFragment.cpp
    @author Stan Kladko
    @date 2019
*/
#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/ParsingException.h"

#include "BlockProposalFragment.h"

BlockProposalFragment::BlockProposalFragment( const block_id& _blockId,
    const uint64_t _totalFragments, const fragment_index& _fragmentIndex,
    const ptr< vector< uint8_t > >& _data, uint64_t _blockSize, const string& _blockHash )
    : data( _data ),
      blockId( _blockId ),
      blockSize( _blockSize ),
      blockHash( _blockHash ),
      totalFragments( _totalFragments ),
      fragmentIndex( _fragmentIndex ) {
    CHECK_ARGUMENT( !_blockHash.empty() );
    CHECK_ARGUMENT( _data );
    CHECK_ARGUMENT( _totalFragments > 0 );
    CHECK_ARGUMENT( _fragmentIndex <= _totalFragments );
    CHECK_ARGUMENT( _blockId > 0 );
    CHECK_ARGUMENT( _data->size() > 0 );

    if ( _data->size() < 3 ) {
        BOOST_THROW_EXCEPTION( ParsingException(
            "Data fragment too short:" + to_string( _data->size() ), __CLASS_NAME__ ) );
    }

    if ( _data->front() != '<' ) {
        BOOST_THROW_EXCEPTION(
            ParsingException( "Data fragment does not start with <", __CLASS_NAME__ ) );
    }

    if ( _data->back() != '>' ) {
        BOOST_THROW_EXCEPTION(
            ParsingException( "Data fragment does not end with >", __CLASS_NAME__ ) );
    }
}

uint64_t BlockProposalFragment::getBlockSize() const {
    return blockSize;
}

string BlockProposalFragment::getBlockHash() const {
    CHECK_STATE( !blockHash.empty() );
    return blockHash;
}


block_id BlockProposalFragment::getBlockId() const {
    return blockId;
}

uint64_t BlockProposalFragment::getTotalFragments() const {
    return totalFragments;
}

fragment_index BlockProposalFragment::getIndex() const {
    return fragmentIndex;
}

ptr< vector< uint8_t > > BlockProposalFragment::serialize() const {
    CHECK_STATE( data );
    return data;
}