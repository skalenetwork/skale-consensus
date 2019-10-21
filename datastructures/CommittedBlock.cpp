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


#include "../Log.h"
#include "../SkaleCommon.h"
#include "../thirdparty/json.hpp"
#include "../crypto/CryptoManager.h"

#include "../abstracttcpserver/ConnectionStatus.h"
#include "../chains/Schain.h"
#include "../crypto/SHAHash.h"
#include "../exceptions/NetworkProtocolException.h"
#include "../exceptions/ParsingException.h"
#include "../exceptions/InvalidStateException.h"
#include "../exceptions/ExitRequestedException.h"
#include "../headers/CommittedBlockHeader.h"


#include "../datastructures/Transaction.h"
#include "../network/Buffer.h"
#include "TransactionList.h"
#include "BlockProposalFragment.h"
#include "CommittedBlock.h"


CommittedBlock::CommittedBlock(ptr< BlockProposal > _p )
    : BlockProposal(_p->getSchainID(), _p->getProposerNodeID(),
          _p->getBlockID(), _p->getProposerIndex(), _p->getTransactionList(), _p->getTimeStamp(),
          _p->getTimeStampMs() ) {
    this->signature = _p->getSignature();
    CHECK_STATE(signature != nullptr);
}


ptr< vector< uint8_t > > CommittedBlock::getSerialized() {



    lock_guard<recursive_mutex> lock(m);

    if (serializedBlock != nullptr)
        return serializedBlock;

    auto header = make_shared< CommittedBlockHeader >( *this );

    auto buf = header->toBuffer();


    CHECK_STATE( buf->getBuf()->at( sizeof( uint64_t ) ) == '{' );
    CHECK_STATE( buf->getBuf()->at( buf->getCounter() - 1 ) == '}' );


    auto block = make_shared< vector< uint8_t > >();

    block->insert(
        block->end(), buf->getBuf()->begin(), buf->getBuf()->begin() + buf->getCounter() );


    auto serializedList = transactionList->serialize( true );
    assert(serializedList->front() == '<' );
    assert(serializedList->back() == '>' );


    block->insert( block->end(), serializedList->begin(), serializedList->end() );

    if ( transactionList->size() == 0 ) {
        CHECK_STATE( block->size() == buf->getCounter() + 2 );
    }

    serializedBlock = block;


    assert(block->at(sizeof( uint64_t )) == '{' );
    assert(block->back() == '>' );

    return block;
}

void CommittedBlock::serializedSanityCheck(ptr< vector< uint8_t > > _serializedBlock) {
        CHECK_STATE( _serializedBlock->at(sizeof( uint64_t )) == '{' );
        CHECK_STATE( _serializedBlock->back() == '>' );
};



ptr<CommittedBlock> CommittedBlock::deserialize(ptr<vector<uint8_t> > _serializedBlock,
        ptr<CryptoManager> _manager) {
    auto block = ptr< CommittedBlock >( new CommittedBlock( 0, 0 ) );

    uint64_t headerSize = 0;

    CHECK_ARGUMENT( _serializedBlock != nullptr );


    auto size = _serializedBlock->size();

    CHECK_ARGUMENT2(
        size >= sizeof( headerSize ) + 2, "Serialized block too small:" + to_string( size ) );


    using boost::iostreams::array_source;
    using boost::iostreams::stream;

    array_source src( ( char* ) _serializedBlock->data(), _serializedBlock->size() );

    stream< array_source > in( src );

    in.read( ( char* ) &headerSize, sizeof( headerSize ) ); /* Flawfinder: ignore */


    CHECK_STATE2( headerSize >= 2 && headerSize + sizeof( headerSize ) <= _serializedBlock->size(),
        "Invalid header size" + to_string( headerSize ) );


    CHECK_STATE( headerSize <= MAX_BUFFER_SIZE );

    CHECK_STATE( _serializedBlock->at( headerSize + sizeof( headerSize ) ) == '<' );
    CHECK_STATE( _serializedBlock->at(sizeof( headerSize )) == '{' );
    CHECK_STATE( _serializedBlock->back() == '>' );

    auto header = make_shared< string >( headerSize, ' ' );

    in.read( ( char* ) header->c_str(), headerSize ); /* Flawfinder: ignore */

    ptr<CommittedBlockHeader> blockHeader;

    try {
        blockHeader = block->parseBlockHeader( header );
    } catch (ExitRequestedException &) {throw;} catch (...) {
        throw_with_nested( ParsingException(
            "Could not parse committed block header: \n" + *header, __CLASS_NAME__ ) );
    }


    try {
        block->transactionList = TransactionList::deserialize(
            blockHeader->getTransactionSizes(), _serializedBlock, headerSize + sizeof( headerSize ), true );
    } catch ( Exception& e ) {
        throw_with_nested(
            ParsingException( "Could not parse transactions after header. Header: \n" + *header +
                                  " Transactions size:" + to_string( _serializedBlock->size() ),
                __CLASS_NAME__ ) );
    }



    _manager->verifyProposalECDSA(block.get(), blockHeader->getBlockHash(), blockHeader->getSignature());

    return block;
}


ptr<CommittedBlock>
CommittedBlock::defragment(ptr<BlockProposalFragmentList> _fragmentList, ptr<CryptoManager> _cryptoManager) {
    try {
        return deserialize(_fragmentList->serialize(), _cryptoManager);
    } catch ( Exception& e ) {
        Exception::logNested(e);
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


ptr<CommittedBlockHeader> CommittedBlock::parseBlockHeader(const shared_ptr< string >& header ) {
    CHECK_ARGUMENT( header != nullptr );
    CHECK_ARGUMENT( header->size() > 2 );
    CHECK_ARGUMENT2( header->at( 0 ) == '{', "Block header does not start with {" );
    CHECK_ARGUMENT2( header->at( header->size() - 1 ) == '}', "Block header does not end with }" );

    auto js = nlohmann::json::parse( *header );
    return make_shared<CommittedBlockHeader>(js);

}
CommittedBlock::CommittedBlock( uint64_t timeStamp, uint32_t timeStampMs )
    : BlockProposal( timeStamp, timeStampMs ) {}


CommittedBlock::CommittedBlock( const schain_id& sChainId, const node_id& proposerNodeId,
    const block_id& blockId, const schain_index& proposerIndex,
    const ptr< TransactionList >& transactions, uint64_t timeStamp, __uint32_t timeStampMs,
    ptr<string> _signature)
    : BlockProposal( sChainId, proposerNodeId, blockId, proposerIndex, transactions, timeStamp,
          timeStampMs ) {
    CHECK_ARGUMENT(_signature != nullptr);
    this->signature = _signature;

}


ptr< CommittedBlock > CommittedBlock::createRandomSample( uint64_t _size,
    boost::random::mt19937& _gen, boost::random::uniform_int_distribution<>& _ubyte,
    block_id _blockID ) {
    auto list = TransactionList::createRandomSample( _size, _gen, _ubyte );

    static uint64_t MODERN_TIME = 1547640182;

    return make_shared< CommittedBlock >( 1, 1, _blockID, 1, list, MODERN_TIME + 1, 1, nullptr );
}

ptr<BlockProposalFragment> CommittedBlock::getFragment(uint64_t _totalFragments, fragment_index _index) {

    CHECK_ARGUMENT(_totalFragments > 0);
    CHECK_ARGUMENT(_index <= _totalFragments);

    lock_guard<recursive_mutex> lock(m);

    auto sBlock = getSerialized();


    auto blockSize = sBlock->size();

    uint64_t fragmentStandardSize;

    if (blockSize % _totalFragments == 0 ) {
        fragmentStandardSize = sBlock->size() / _totalFragments;
    } else {
        fragmentStandardSize = sBlock->size() / _totalFragments + 1;
    }

    auto startIndex = fragmentStandardSize * ((uint64_t ) _index - 1);


    auto fragmentData = make_shared<vector<uint8_t>>();
    fragmentData->reserve(fragmentStandardSize + 2);

    fragmentData->push_back('<');


    if (_index == _totalFragments) {
        fragmentData->insert(fragmentData->begin() + 1, sBlock->begin() + startIndex,
                             sBlock->end());

    } else {
        fragmentData->insert(fragmentData->begin() + 1, sBlock->begin() + startIndex,
                             sBlock->begin() + startIndex + fragmentStandardSize);
    }

    fragmentData->push_back('>');


    return make_shared<BlockProposalFragment>(getBlockID(), _totalFragments, _index, fragmentData,
                                              sBlock->size(), getHash()->toHex());
}
