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

    @file CommittedBlockList.cpp
    @author Stan Kladko
    @date 2018
*/


#include "../Log.h"
#include "../SkaleCommon.h"
#include "../crypto/SHAHash.h"
#include "../exceptions/NetworkProtocolException.h"
#include "CommittedBlock.h"

#include "CommittedBlockList.h"


CommittedBlockList::CommittedBlockList( ptr< vector< ptr< CommittedBlock > > > _blocks ) {
    ASSERT( _blocks );
    ASSERT( _blocks->size() > 0 );

    blocks = _blocks;
}


CommittedBlockList::CommittedBlockList( ptr< vector< uint64_t > > _blockSizes,
    ptr< vector< uint8_t > > _serializedBlocks, uint64_t _offset ) {
    CHECK_ARGUMENT( _serializedBlocks->at( _offset ) == '[' );
    CHECK_ARGUMENT( _serializedBlocks->at( _serializedBlocks->size() - 1 ) == ']' );


    size_t index = _offset + 1;
    uint64_t counter = 0;

    blocks = make_shared< vector< ptr< CommittedBlock > > >();

    for ( auto&& size : *_blockSizes ) {
        auto endIndex = index + size;

        ASSERT( endIndex <= _serializedBlocks->size() );

        auto blockData = make_shared< vector< uint8_t > >(
            _serializedBlocks->begin() + index, _serializedBlocks->begin() + endIndex );

        auto block = CommittedBlock::deserialize( blockData );

        blocks->push_back( block );

        index = endIndex;

        counter++;
    }
};


ptr< vector< ptr< CommittedBlock > > > CommittedBlockList::getBlocks() {
    return blocks;
}

shared_ptr< vector< uint8_t > > CommittedBlockList::serialize() {
    auto serializedBlocks = make_shared< vector< uint8_t > >();

    serializedBlocks->push_back( '[' );

    for ( auto&& block : *blocks ) {
        auto data = block->serialize();
        serializedBlocks->insert( serializedBlocks->end(), data->begin(), data->end() );
    }

    serializedBlocks->push_back( ']' );

    return serializedBlocks;
}


ptr< CommittedBlockList > CommittedBlockList::createRandomSample( uint64_t _size,
    boost::random::mt19937& _gen, boost::random::uniform_int_distribution<>& _ubyte ) {
    auto blcks = make_shared< vector< ptr< CommittedBlock > > >();

    for ( uint32_t i = 0; i < _size; i++ ) {
        auto block = CommittedBlock::createRandomSample( _size - 1, _gen, _ubyte, i );
        blcks->push_back( block );
    }

    return make_shared< CommittedBlockList >( blcks );
}
ptr< CommittedBlockList > CommittedBlockList::deserialize( ptr< vector< uint64_t > > _blockSizes,
    ptr< vector< uint8_t > > _serializedBlocks, uint64_t _offset ) {
    return ptr< CommittedBlockList >(
        new CommittedBlockList( _blockSizes, _serializedBlocks, _offset ) );
}

ptr< vector< uint64_t > > CommittedBlockList::createSizes() {
    auto ret = make_shared< vector< uint64_t > >();

    for ( auto&& block : *blocks ) {
        ret->push_back( block->serialize()->size() );
    }

    return ret;
};
