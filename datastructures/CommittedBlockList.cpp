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

#include "Log.h"
#include "SkaleCommon.h"

#include "crypto/CryptoManager.h"
#include "crypto/BLAKE3Hash.h"
#include "exceptions/InvalidStateException.h"
#include "chains/Schain.h"
#include "CommittedBlock.h"
#include "CommittedBlockList.h"

CommittedBlockList::CommittedBlockList(const ptr< vector< ptr< CommittedBlock > > >& _blocks ) {
    CHECK_ARGUMENT( _blocks );
    CHECK_ARGUMENT( _blocks->size() > 0 );

    blocks = _blocks;
}


CommittedBlockList::CommittedBlockList(const ptr< CryptoManager >& _cryptoManager,
    const ptr<vector<uint64_t>>& _blockSizes, const ptr<vector<uint8_t>>& _serializedBlocks,
    uint64_t _offset ) {
    CHECK_ARGUMENT( _cryptoManager );
    CHECK_ARGUMENT( _blockSizes );
    CHECK_ARGUMENT( _serializedBlocks );


    CHECK_ARGUMENT( _serializedBlocks->at( _offset ) == '[' );
    CHECK_ARGUMENT( _serializedBlocks->at( _serializedBlocks->size() - 1 ) == ']' );

    uint64_t counter = 0;
    size_t index = 0;
    size_t endIndex = 0;

    try {
        index = _offset + 1;

        blocks = make_shared< vector< ptr< CommittedBlock > > >();

        LOG(info, "Deserializing " + to_string( _blockSizes->size() ) + " blocks from " +
            string( _cryptoManager->getSchain()->getLastCommittedBlockID() + 1 ) + " to " +
            string( _cryptoManager->getSchain()->getLastCommittedBlockID() + _blockSizes->size() ));

        for ( auto&& size : *_blockSizes ) {
            endIndex = index + size;

            CHECK_STATE( endIndex <= _serializedBlocks->size() );

            auto blockData = make_shared<vector<uint8_t>>(
                _serializedBlocks->begin() + index, _serializedBlocks->begin() + endIndex );

            auto block = CommittedBlock::deserialize( blockData, _cryptoManager, true );

            if (!_cryptoManager->getSchain()->isLegacy())
                CHECK_STATE(!block->getDaSig().empty())

            blocks->push_back(block);

            index = endIndex;

            counter++;
        }
    } catch ( ... ) {
        LOG(err, "Successfully deserialized " + to_string(counter) + " blocks, got exception on block " +
            to_string( _cryptoManager->getSchain()->getLastCommittedBlockID() + counter + 1 ));
        throw_with_nested( InvalidStateException(
            "Could not create block list. \n"
                "LIST_SIZE:" + to_string(_blockSizes->size()) +
                ":SERIALIZED_BLOCK_SIZE:" + to_string(_serializedBlocks->size()) +
                ":OFFSET:" + to_string(_offset) +
                ":COUNTER:" + to_string(counter) +
                ":INDEX:" + to_string(index) +
                ":END_INDEX:" + to_string(endIndex), __CLASS_NAME__ ) );
    }
};


ptr< vector< ptr< CommittedBlock > > > CommittedBlockList::getBlocks() {
    return blocks;
}

shared_ptr<vector<uint8_t>> CommittedBlockList::serialize() {

    auto serializedBlocks = make_shared<vector<uint8_t>>();

    serializedBlocks->push_back( '[' );

    {
        LOCK( m )

        for ( auto&& block : *blocks ) {
            auto data = block->serialize();
            serializedBlocks->insert( serializedBlocks->end(), data->begin(), data->end() );
        }
    }

    serializedBlocks->push_back( ']' );

    return serializedBlocks;
}


ptr< CommittedBlockList > CommittedBlockList::createRandomSample(
    const ptr< CryptoManager >& _cryptoManager, uint64_t _size, boost::random::mt19937& _gen,
    boost::random::uniform_int_distribution<>& _ubyte ) {
    auto blcks = make_shared< vector< ptr< CommittedBlock > > >();

    for ( uint32_t i = 0; i < _size; i++ ) {
        auto block =
            CommittedBlock::createRandomSample( _cryptoManager, _size - 1, _gen, _ubyte, i );
        blcks->push_back( block );
    }

    return make_shared< CommittedBlockList >( blcks );
}

ptr< CommittedBlockList > CommittedBlockList::deserialize(const ptr< CryptoManager >& _cryptoManager,
    const ptr<vector<uint64_t>>& _blockSizes, const ptr<vector<uint8_t>>& _serializedBlocks,
    uint64_t _offset ) {

    if (_serializedBlocks->at(0) != '[') {
        BOOST_THROW_EXCEPTION(
                InvalidStateException("Serialized blocks do not start with [", __CLASS_NAME__));
    }

    return ptr< CommittedBlockList >(
        new CommittedBlockList( _cryptoManager, _blockSizes, _serializedBlocks, _offset ) );
}

ptr<vector<uint64_t>> CommittedBlockList::createSizes() {

    auto ret = make_shared< vector< uint64_t > >();

    LOCK(m)

    for ( auto&& block : *blocks ) {
        ret->push_back(block->serialize()->size() );
    }

    return ret;
};
