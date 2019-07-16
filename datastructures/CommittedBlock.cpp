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

    @file CommittedBlock.cpp
    @author Stan Kladko
    @date 2018
*/

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>


#include "../thirdparty/json.hpp"
#include "../SkaleCommon.h"
#include "../Log.h"

#include "../chains/Schain.h"
#include "../abstracttcpserver/ConnectionStatus.h"
#include "../crypto/SHAHash.h"
#include "../exceptions/NetworkProtocolException.h"
#include "../exceptions/ParsingException.h"
#include "../headers/CommittedBlockHeader.h"


#include "../datastructures/Transaction.h"
#include "../network/Buffer.h"
#include "CommittedBlock.h"
#include "TransactionList.h"






CommittedBlock::CommittedBlock( Schain& _sChain, ptr< BlockProposal > _p )
    : BlockProposal( _sChain.getSchainID(), _sChain.getNodeIDByIndex(_p->getProposerIndex()),_p->getBlockID(), _p->getProposerIndex(), _p->getTransactionList(),
          _p->getTimeStamp(), _p->getTimeStampMs() ) {}


ptr< vector< uint8_t > > CommittedBlock::serialize() {

    auto header = make_shared< CommittedBlockHeader >( *this );

    auto buf = header->toBuffer();




    CHECK_STATE( buf->getBuf()->at( sizeof( uint64_t ) ) == '{' );
    CHECK_STATE( buf->getBuf()->at( buf->getCounter() - 1 ) == '}' );


    auto block = make_shared< vector< uint8_t > >();

    block->insert(
        block->end(), buf->getBuf()->begin(), buf->getBuf()->begin() + buf->getCounter() );


    auto serializedList = transactionList->serialize(true);

    block->insert(block->end(), serializedList->begin(), serializedList->end());


    CHECK_STATE( block->at( sizeof( uint64_t ) ) == '{' );


    return block;
}


ptr< CommittedBlock > CommittedBlock::deserialize( ptr< vector< uint8_t > > _serializedBlock ) {
    auto block = ptr< CommittedBlock >( new CommittedBlock( 0, 0 ) );




    uint64_t headerSize = 0;

    CHECK_ARGUMENT( _serializedBlock != nullptr );



    auto size = _serializedBlock->size();

    CHECK_ARGUMENT2(size >= sizeof( headerSize ) + 2,"Serialized block too small:" + to_string( size ));


    using boost::iostreams::array_source;
    using boost::iostreams::stream;

    array_source src( ( char* ) _serializedBlock->data(), _serializedBlock->size() );

    stream< array_source > in( src );

    in.read( ( char* ) &headerSize, sizeof( headerSize ) ); /* Flawfinder: ignore */


    CHECK_STATE2(headerSize >= 2 && headerSize + sizeof( headerSize ) <= _serializedBlock->size(),
            "Invalid header size" + to_string( headerSize ));


    CHECK_STATE( headerSize <= MAX_BUFFER_SIZE );

    CHECK_STATE(_serializedBlock->at(headerSize + sizeof(headerSize) ) == '<');

    auto header = make_shared< string >( headerSize, ' ' );

    in.read( ( char* ) header->c_str(), headerSize ); /* Flawfinder: ignore */

    ptr< vector< size_t > > transactionSizes;

    try {
        transactionSizes = block->parseBlockHeader( header );
    } catch ( ... ) {
        throw_with_nested( ParsingException(
            "Could not parse committed block header: \n" + *header, __CLASS_NAME__ ) );
    }



    try {
        block->transactionList =
            TransactionList::deserialize( transactionSizes, _serializedBlock, headerSize
                + sizeof(headerSize), true );
    } catch ( ... ) {
        throw_with_nested( ParsingException(
            "Could not parse transactions after header. Header: \n" + *header, __CLASS_NAME__ ) );
    }

    block->calculateHash();

    return block;
}

ptr< vector< size_t > > CommittedBlock::parseBlockHeader( const shared_ptr< string >& header ) {
    CHECK_ARGUMENT( header != nullptr );

    CHECK_ARGUMENT( header->size() > 2 );

    CHECK_ARGUMENT2(header->at( 0 ) == '{', "Block header does not start with {");

    CHECK_ARGUMENT2(header->at( header->size() - 1 ) == '}',"Block header does not end with }");


    auto transactionSizes = make_shared< vector< size_t > >();

    size_t totalSize = 0;

    auto js = nlohmann::json::parse( *header );

    proposerIndex = schain_index( Header::getUint64( js, "proposerIndex" ) );
    proposerNodeID = node_id( Header::getUint64( js, "proposerNodeID" ) );
    blockID = block_id( Header::getUint64( js, "blockID" ) );
    schainID = schain_id( Header::getUint64( js, "schainID" ) );
    timeStamp = Header::getUint64( js, "timeStamp" );
    timeStampMs = Header::getUint32( js, "timeStampMs" );

    transactionCount = js["sizes"].size();
    hash = SHAHash::fromHex( Header::getString( js, "hash" ) );


    Header::nullCheck( js, "sizes" );
    nlohmann::json jsonTransactionSizes = js["sizes"];
    transactionCount = jsonTransactionSizes.size();

    for ( auto&& jsize : jsonTransactionSizes ) {
        transactionSizes->push_back( jsize );
        totalSize += ( size_t ) jsize;
    }

    return transactionSizes;
}
CommittedBlock::CommittedBlock( uint64_t timeStamp, uint32_t timeStampMs )
    : BlockProposal( timeStamp, timeStampMs ) {}


CommittedBlock::CommittedBlock( const schain_id& sChainId, const node_id& proposerNodeId,
    const block_id& blockId, const schain_index& proposerIndex,
    const ptr< TransactionList >& transactions, uint64_t timeStamp, __uint32_t timeStampMs )
    : BlockProposal( sChainId, proposerNodeId, blockId, proposerIndex, transactions, timeStamp,
          timeStampMs ) {}



ptr< CommittedBlock > CommittedBlock::createRandomSample(uint64_t _size, boost::random::mt19937& _gen,
                                                     boost::random::uniform_int_distribution<>& _ubyte,
                                                     block_id _blockID) {
    auto list = TransactionList::createRandomSample(_size, _gen, _ubyte );

    static uint64_t MODERN_TIME = 1547640182;

    return make_shared< CommittedBlock >( 1, 1, _blockID, 1, list, MODERN_TIME + 1, 1 );
};