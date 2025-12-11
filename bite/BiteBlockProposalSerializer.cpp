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
#include "BiteBlockProposalSerializer.h"

ptr<std::vector<uint8_t>> BiteBlockProposalSerializer::serializeTransactionsAndCompleteSerialization(
    ptr<BasicHeader> _blockHeader, ptr<TransactionList> transactionList) {

    CHECK_STATE(_blockHeader);
    CHECK_STATE(transactionList);

    auto buf = _blockHeader->toBuffer();
    CHECK_STATE(buf);

    auto* headerBuf = buf->getBuf()->data();
    auto headerSize = buf->getCounter();
    CHECK_STATE(headerBuf[sizeof(uint64_t)] == '{');
    CHECK_STATE(headerBuf[headerSize - 1] == '}');

    // Reuse builder (thread-local, fast path)
    thread_local flatbuffers::FlatBufferBuilder builder(1024 * 1024);
    builder.Clear();

    // Reserve + emplace (faster than push_back)
    const auto& items = *transactionList->getItems();
    std::vector<flatbuffers::Offset<skale_fb::Transaction>> transactionsVec;
    transactionsVec.reserve(items.size());

    for (const auto& tx : items) {
        const auto& txDataVec = *tx->getData();
        auto txData = builder.CreateVector(txDataVec.data(), txDataVec.size());
        transactionsVec.emplace_back(skale_fb::CreateTransaction(builder, txData));
    }

    auto headerOffset = builder.CreateVector(
        headerBuf + sizeof(uint64_t), headerSize - sizeof(uint64_t));
    auto transactionsOffset = builder.CreateVector(transactionsVec);

    auto proposalOffset = CreateBlockProposal(builder, headerOffset, transactionsOffset);
    builder.Finish(proposalOffset);

    const uint8_t* raw = builder.GetBufferPointer();
    size_t size = builder.GetSize();

    // Slightly faster than resize + memcpy
    auto buffer = std::make_shared<std::vector<uint8_t>>(raw, raw + size);


    return buffer;
}


ptr< BlockProposal > BiteBlockProposalSerializer::deserialize(
    const ptr< vector< uint8_t > >& _serializedProposal, const ptr< CryptoManager >& _manager,
    bool _verifySig ) {
    CHECK_ARGUMENT( _serializedProposal );


    const skale_fb::BlockProposal* fbProposal = nullptr;

    VERIFY_AND_PARSE_FLATBUFFER_FROM_VECTOR( *_serializedProposal, BlockProposal, fbProposal );


    // Extract block header data
    auto fbHeaderVec = fbProposal->block_header();
    size_t headerSize = fbHeaderVec->size();
    CHECK_STATE(fbHeaderVec->data())
    std::string_view headerView(reinterpret_cast<const char*>(fbHeaderVec->data()), headerSize);


    CHECK_STATE( !headerView.empty() );

    ptr< BlockProposalHeader > blockHeader;

    try {
        blockHeader = BlockProposal::parseBlockHeader( headerView );
        CHECK_STATE( blockHeader );
    } catch ( ... ) {
        throw_with_nested(
            ParsingException( "Could not parse block header:\n " + string(headerView), __CLASS_NAME__ ) );
    }


    auto sig = blockHeader->getSignature();

    CHECK_STATE( !sig.empty() );
    // Reconstruct transactions
    auto fbTransactions = fbProposal->transactions();
    CHECK_STATE( fbTransactions );

    auto transactions = make_shared< vector< ptr< Transaction > > >();
    transactions->reserve(fbTransactions->size());

    for ( const auto* tx : *fbTransactions ) {
        CHECK_STATE( tx && tx->data() );
        // Copy transaction data
        auto rawData = tx->data()->data();
        auto txData = make_shared< vector< uint8_t > >( rawData,
            rawData + tx->data()->size() );
        auto txObj = make_shared< Transaction >( txData, false );
        transactions->push_back( txObj );
    }


    auto list = std::make_shared< TransactionList >( transactions );


    auto proposal =
        BlockProposal::makeFromSerialized( blockHeader->getSchainID(), blockHeader->getProposerNodeId(),
            blockHeader->getBlockID(),
#ifdef BITE
            blockHeader->getEpochID(),
#endif
            blockHeader->getProposerIndex(), list,
            blockHeader->getStateRoot(), blockHeader->getTimeStamp(), blockHeader->getTimeStampMs(),
            blockHeader->getSignature(), nullptr );
    proposal->setCachedSerializedProposal(_serializedProposal);
    // default blocks are not ecdsa signed
    if ( _verifySig && ( blockHeader->getProposerIndex() != 0 ) ) {
        try {
            _manager->verifyProposalECDSA(
                proposal, blockHeader->getBlockHash(), blockHeader->getSignature() );
        } catch ( ... ) {
            CONS_LOG( err, "Block proposer ecdsa signature did not verify for"
                          << to_string( ( uint64_t ) proposal->getProposerIndex() ) );
            throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
        }
    }

    return proposal;
};

void BiteBlockProposalSerializer::serializedSanityCheck( const ptr< vector< uint8_t > >& _serializedBlock ) {
    CHECK_STATE(_serializedBlock);
    // Verify the resulting buffer before returning
    flatbuffers::Verifier verifier(_serializedBlock->data(), _serializedBlock->size());
    CHECK_STATE(skale_fb::VerifyBlockProposalBuffer(verifier));
}
