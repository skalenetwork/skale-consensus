#include <flatbuffers/flatbuffers.h>
#include "Log.h"
#include "crypto/CryptoManager.h"
#include "flatb/FlatBufferRequest.h"
#include "flatb/common_structures_generated.h"
#include "flatb/committed_bloc_generated.h"
#include "headers/BlockProposalHeader.h"
#include "exceptions/ParsingException.h"
#include "network/Buffer.h"
#include "datastructures/Transaction.h"
#include "datastructures/TransactionList.h"
#include "headers/CommittedBlockHeader.h"
#include "BITECommittedBlock.h"


BITECommittedBlock::BITECommittedBlock( const schain_id& schainId, const node_id& proposerNodeId,
    const block_id& blockId, const schain_index& proposerIndex,
    const ptr< TransactionList >& transactions, const u256& stateRoot, uint64_t timeStamp,
    __uint32_t timeStampMs, const string& signature, const string& thresholdSig,
    const string& daSig )
    : CommittedBlock( schainId, proposerNodeId, blockId, proposerIndex, transactions, stateRoot,
          timeStamp, timeStampMs, signature, thresholdSig, daSig ) {}


ptr< std::vector< uint8_t > > BITECommittedBlock::serializeTransactionsAndCompleteSerialization(
    ptr< BasicHeader > _blockHeader ) {
    CHECK_STATE( _blockHeader );
    CHECK_STATE( transactionList );
    
    auto buf = _blockHeader->toBuffer();
    CHECK_STATE( buf );
    CHECK_STATE( buf->getBuf()->at( sizeof( uint64_t ) ) == '{' );
    CHECK_STATE( buf->getBuf()->at( buf->getCounter() - 1 ) == '}' );

    // Preallocate ~1MB (tune as needed)
    flatbuffers::FlatBufferBuilder builder( 1024 * 1024 );

    // Serialize each transaction
    auto& items = *transactionList->getItems();
    std::vector< flatbuffers::Offset< skale_fb::Transaction > > transactionsVec;
    transactionsVec.reserve( items.size() );

    for ( const auto& tx : items ) {
        const auto& txDataVec = *tx->getData();
        auto txData = builder.CreateVector( txDataVec.data(), txDataVec.size() );
        transactionsVec.push_back( skale_fb::CreateTransaction( builder, txData ) );
    }

    // Serialize block header buffer directly. It starts with uint64_t size and then
    // includes the actual header. We do not need the size
    auto headerOffset = builder.CreateVector(
        buf->getBuf()->data() + sizeof( uint64_t ), buf->getCounter() - sizeof( uint64_t ) );
    auto transactionsOffset = builder.CreateVector( transactionsVec );

    // Finalize block
    auto blockOffset = CreateCommittedBlock( builder, headerOffset, transactionsOffset );
    builder.Finish( blockOffset );


    uint8_t* raw = builder.GetBufferPointer();
    size_t size = builder.GetSize();


    // 🔍 Verify the resulting buffer before returning
    flatbuffers::Verifier verifier(raw, size);
    CHECK_STATE(skale_fb::VerifyCommittedBlockBuffer(verifier));

    auto buffer = std::make_shared< std::vector< uint8_t > >();
    buffer->resize( size );
    std::memcpy( buffer->data(), raw, size );  // unavoidable copy if caller requires vector

    return buffer;
}


void BITECommittedBlock::serializedSanityCheck( const ptr< vector< uint8_t > >& _serializedBlock ) {
    // 🔍 Verify the resulting buffer before returning
    CHECK_STATE(_serializedBlock);
    flatbuffers::Verifier verifier(_serializedBlock->data(), _serializedBlock->size());
    CHECK_STATE(skale_fb::VerifyCommittedBlockBuffer(verifier));
}



ptr< CommittedBlock > BITECommittedBlock::deserialize( const ptr< vector< uint8_t > >& _serializedBlock,
const ptr< CryptoManager >& _manager, bool _verifySig ) {

    CHECK_ARGUMENT( _serializedBlock );
    CHECK_ARGUMENT( _manager );


    const skale_fb::CommittedBlock* fbBlock = nullptr;

    VERIFY_AND_PARSE_FLATBUFFER_FROM_VECTOR( *_serializedBlock, CommittedBlock, fbBlock );

    // Extract block header data
    auto fbHeaderVec = fbBlock->block_header();
    CHECK_STATE( fbHeaderVec );
    size_t headerSize = fbHeaderVec->size();
    CHECK_STATE(fbHeaderVec->data())
    std::string headerStr(reinterpret_cast<const char*>(fbHeaderVec->data()), headerSize);


    ptr< CommittedBlockHeader > blockHeader;

    try {
        blockHeader = CommittedBlock::parseBlockHeader( headerStr );
        CHECK_STATE( blockHeader );
    } catch ( ... ) {
        throw_with_nested( ParsingException(
            "Could not parse committed block header: \n" + headerStr, __CLASS_NAME__ ) );
    }


    auto fbTransactions = fbBlock->transactions();
    CHECK_STATE( fbTransactions );

    auto transactions = make_shared< vector< ptr< Transaction > > >();

    for ( const auto* tx : *fbTransactions ) {
        CHECK_STATE( tx && tx->data() );

        // Copy transaction data
        auto txData = make_shared< vector< uint8_t > >( tx->data()->begin(), tx->data()->end() );
        auto txObj = make_shared< Transaction >( txData, false );  // hypothetical
        transactions->push_back( txObj );
    }

    auto list = std::make_shared< TransactionList >( transactions );



    ptr< CommittedBlock > block = nullptr;

    try {
        block = CommittedBlock::make( blockHeader->getSchainID(), blockHeader->getProposerNodeId(),
            blockHeader->getBlockID(), blockHeader->getProposerIndex(), list,
            blockHeader->getStateRoot(), blockHeader->getTimeStamp(), blockHeader->getTimeStampMs(),
            blockHeader->getSignature(), blockHeader->getThresholdSig(), blockHeader->getDaSig() );
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( "Could not make block", __CLASS_NAME__ ) );
    }

    CHECK_STATE( block );

    if ( !_verifySig ) {
        return block;
    }

    // now verify block proposer signature and block signature
    // default blocks are not ecdsa signed
    if ( ( blockHeader->getProposerIndex() != 0 ) ) {
        try {
            _manager->verifyProposalECDSA(
                block, blockHeader->getBlockHash(), blockHeader->getSignature() );
        } catch ( ... ) {
            LOG( err, "Block ECDSA signature did not verify in deserialization" );
            throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
        }
    }

    try {
        block->verifyBlockSig( _manager );
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__,
            __CLASS_NAME__ +
                string( " Block threshold signature did not verify in deserialization" ) ) );
    }

    try {
            block->verifyDaSig( _manager );
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__,
            __CLASS_NAME__ + string( " Block da signature did not verify in deserialization" ) ) );
    }

    return block;

}

