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

#ifdef BITE
#include "flatb/FlatBufferRequest.h"
#include "flatb/committed_block_fragment_generated.h"
#include "crypto/AESKeyDecryptionShare.h"
#include "crypto/AESKeyDecryptionShareList.h"
#include "bite/BiteBlockProposalSerializer.h"
#include "bite/BiteManager.h"
#include "bite/BiteAESDecryptionShareSerializer.h"
#endif

#include "BlockProposalFragment.h"


BlockProposalFragment::BlockProposalFragment( const block_id& _blockId,
#ifdef BITE
    const schain_index _proposerIndex, const schain_index _decryptorIndex,
#endif
    const uint64_t _totalFragments, const fragment_index& _fragmentIndex,
    const ptr< vector< uint8_t > >& _data, uint64_t _blockSize, const string& _blockHash )
    : data( _data ),
      blockId( _blockId ),
      proposerIndex( _proposerIndex ),
      decryptorIndex( _decryptorIndex ),
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


#ifdef BITE
    deserializeFromFlatBuffer();
#else

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
#endif
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

ptr< vector< uint8_t > > BlockProposalFragment::serialize() {
#ifdef BITE


    if ( auto cachedSerializedBuffer = std::atomic_load( &_fbSerializedBlockFragment ) ) {
        if ( cachedSerializedBuffer ) {
            return cachedSerializedBuffer;
        }
    }

    thread_local flatbuffers::FlatBufferBuilder builder( 1024 * 1024 );
    builder.Clear();


    flatbuffers::Offset< flatbuffers::Vector< unsigned char > > fbData;
    if ( data ) {
        fbData = builder.CreateVector( *data );
    }

    // ✅ Create empty vector of raw pointers for Hash*
    auto emptyHashVec = builder.CreateVector< const skale_fb::Hash* >( {} );
    auto emptyDecryptionShares =
        builder.CreateVector< flatbuffers::Offset< skale_fb::DecryptionShare > >( {} );
    auto emptySig = builder.CreateVector( std::vector< uint8_t >{} );


    auto proposalOffset = skale_fb::CreateCommittedBlockFragment(
        builder, emptyHashVec, emptyHashVec, emptyDecryptionShares, emptySig, fbData );
    builder.Finish( proposalOffset );

    const uint8_t* raw = builder.GetBufferPointer();
    size_t size = builder.GetSize();

    // Slightly faster than resize + memcpy
    auto buffer = std::make_shared< std::vector< uint8_t > >( raw, raw + size );

    std::atomic_store( &_fbSerializedBlockFragment, buffer );

    return buffer;

#else
    CHECK_STATE( data );
    return data;
#endif
}


#ifdef BITE
void BlockProposalFragment::deserializeFromFlatBuffer() {
    CHECK_STATE( data )


    VERIFY_AND_PARSE_FLATBUFFER_FROM_VECTOR( *data, CommittedBlockFragment, fbBlockFragment );


    auto fbDecryptionSharesHandle = fbBlockFragment->decryption_shares();

    CHECK_STATE( fbDecryptionSharesHandle );

    decryptionShares = BiteAESDecryptionShareSerializer::getDecryptionShares(
        blockId, proposerIndex, decryptorIndex, fbDecryptionSharesHandle );
}


BlockProposalFragment::BlockProposalFragment( const block_id& _blockId,
#ifdef BITE
    const schain_index _proposerIndex, const schain_index _decryptorIndex,
#endif
    uint64_t _totalFragments, const fragment_index& _fragmentIndex,
    const ptr< vector< uint8_t > >& _data, ptr< AESKeyDecryptionShareList >, uint64_t _blockSize,
    const string& _blockHash )
    : data( _data ),
      blockId( _blockId ),
      proposerIndex( _proposerIndex ),
      decryptorIndex( _decryptorIndex ),
      blockSize( _blockSize ),
      blockHash( _blockHash ),
      totalFragments( _totalFragments ),
      fragmentIndex( _fragmentIndex ) {
    // Reuse builder (thread-local, fast path)
}
const ptr< AESKeyDecryptionShareList >& BlockProposalFragment::getDecryptionShares() const {
    return decryptionShares;
}
#endif
