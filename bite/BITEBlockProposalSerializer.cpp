#include <flatbuffers/flatbuffers.h>
#include "Log.h"
#include "crypto/CryptoManager.h"
#include "flatb/FlatBufferRequest.h"
#include "flatb/common_structures_generated.h"
#include "flatb/block_proposal_generated.h"
#include "headers/BlockProposalHeader.h"
#include "exceptions/ParsingException.h"
#include "network/Buffer.h"
#include "datastructures/Transaction.h"
#include "datastructures/TransactionList.h"
#include "BITEBlockProposalSerializer.h"


ptr< std::vector< uint8_t > >
BITEBlockProposalSerializer::serializeTransactionsAndCompleteSerialization(
    ptr< BasicHeader > _blockHeader, ptr<TransactionList> transactionList ) {
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

    // Finalize proposal
    auto proposalOffset = CreateBlockProposal( builder, headerOffset, transactionsOffset );
    builder.Finish( proposalOffset );


    uint8_t* raw = builder.GetBufferPointer();
    size_t size = builder.GetSize();


    auto buffer = std::make_shared< std::vector< uint8_t > >();
    buffer->resize( size );
    std::memcpy( buffer->data(), raw, size );  // unavoidable copy if caller requires vector

    serializedSanityCheck(buffer);

    return buffer;
}


ptr< BlockProposal > BITEBlockProposalSerializer::deserialize(
    const ptr< vector< uint8_t > >& _serializedProposal, const ptr< CryptoManager >& _manager,
    bool _verifySig ) {
    CHECK_ARGUMENT( _serializedProposal );
    //CHECK_ARGUMENT( _manager );


    const skale_fb::BlockProposal* fbProposal = nullptr;

    VERIFY_AND_PARSE_FLATBUFFER_FROM_VECTOR( *_serializedProposal, BlockProposal, fbProposal );


    // Extract block header data
    auto fbHeaderVec = fbProposal->block_header();
    size_t headerSize = fbHeaderVec->size();
    CHECK_STATE(fbHeaderVec->data())
    std::string headerStr(reinterpret_cast<const char*>(fbHeaderVec->data()), headerSize);


    CHECK_STATE( !headerStr.empty() );

    ptr< BlockProposalHeader > blockHeader;

    try {
        blockHeader = BlockProposal::parseBlockHeader( headerStr );
        CHECK_STATE( blockHeader );
    } catch ( ... ) {
        throw_with_nested(
            ParsingException( "Could not parse block header: \n" + headerStr, __CLASS_NAME__ ) );
    }


    auto sig = blockHeader->getSignature();

    CHECK_STATE( !sig.empty() );
    // Reconstruct transactions
    auto fbTransactions = fbProposal->transactions();
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


    auto proposal =
        BlockProposal::make( blockHeader->getSchainID(), blockHeader->getProposerNodeId(),
            blockHeader->getBlockID(), blockHeader->getProposerIndex(), list,
            blockHeader->getStateRoot(), blockHeader->getTimeStamp(), blockHeader->getTimeStampMs(),
            blockHeader->getSignature(), nullptr );
    // default blocks are not ecdsa signed
    if ( _verifySig && ( blockHeader->getProposerIndex() != 0 ) ) {
        try {
            _manager->verifyProposalECDSA(
                proposal, blockHeader->getBlockHash(), blockHeader->getSignature() );
        } catch ( ... ) {
            LOG( err, "Block proposer ecdsa signature did not verify for"
                          << to_string( ( uint64_t ) proposal->getProposerIndex() ) );
            throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
        }
    }


    proposal->setCachedSerializedProposal(_serializedProposal);

    return proposal;
};

void BITEBlockProposalSerializer::serializedSanityCheck( const ptr< vector< uint8_t > >& _serializedBlock ) {
    CHECK_STATE(_serializedBlock);
    // 🔍 Verify the resulting buffer before returning
    flatbuffers::Verifier verifier(_serializedBlock->data(), _serializedBlock->size());
    CHECK_STATE(skale_fb::VerifyBlockProposalBuffer(verifier));
}
