#include "thirdparty/catch.hpp"

#ifdef BITE

#include "bite/BiteBlockProposalSerializer.h"
#include "headers/BlockProposalHeader.h"
#include "datastructures/BlockProposal.h"
#include "datastructures/TransactionList.h"

#include "BiteTestUtils.h"
#include "libBLS/test/utils.h"

using namespace std;
using namespace BiteTestUtils;

CATCH_TEST_CASE(
    "BiteBlockProposalSerializer serialize and deserialize", "[bite][serializer][proposal]" ) {
    // Setup environment
    ConsensusEngine engine( 0, 100000000 );
    std::shared_ptr< Schain > chain;
    std::shared_ptr< Node > node;
    auto cryptoManager = createTestCryptoManager( chain, node, engine );

    // Setup Proposal Data
    schain_id schainID = 1;
    node_id proposerNodeID = 1;
    block_id blockID = 200;
    epoch_id epochID = 20;
    schain_index proposerIndex = 1;
    u256 stateRoot = 0x123456;
    uint64_t timeStamp = std::time( nullptr );  // Use current time to satisfy MODERN_TIME check
    uint32_t timeStampMs = 456;
    string signature = "mock_proposal_sig";

    // Create Transactions
    auto keys = generateKeys( 1, 1 );
    auto tx1 = buildBite1Transaction( 
        vector< uint8_t >{ 0x01 },      // plain text
        vector< uint8_t >( 20, 0x11 ),  // to
        static_cast< uint64_t >( epochID ), 
        keys.commonPublic 
    );
    auto txVec = make_shared< vector< ptr< Transaction > > >();
    txVec->push_back( tx1 );
    auto txList = make_shared< TransactionList >( txVec );

    // Create Proposal
    auto proposal = BlockProposal::makeFromSerialized(
        schainID, proposerNodeID, blockID, epochID, proposerIndex, txList, stateRoot, timeStamp,
        timeStampMs, signature, nullptr  // cryptoManager null to avoid signing attempts
    );

    auto header = make_shared< BlockProposalHeader >( *proposal );

    // 1. Test Serialization
    ptr< vector< uint8_t > > serialized = nullptr;
    CATCH_REQUIRE_NOTHROW(
        serialized = BiteBlockProposalSerializer::serializeTransactionsAndCompleteSerialization(
            header, txList ) );

    CATCH_REQUIRE( serialized != nullptr );
    CATCH_REQUIRE( serialized->size() > 0 );

    // 2. Test Sanity Check
    CATCH_REQUIRE_NOTHROW( BiteBlockProposalSerializer::serializedSanityCheck( serialized ) );

    // 3. Test Deserialization
    ptr< BlockProposal > deserialized = nullptr;
    bool verifySig = false;

    CATCH_REQUIRE_NOTHROW( deserialized = BiteBlockProposalSerializer::deserialize(
                               serialized, cryptoManager, verifySig ) );

    CATCH_REQUIRE( deserialized != nullptr );
    CATCH_REQUIRE( deserialized->getBlockID() == blockID );
    CATCH_REQUIRE( deserialized->getEpochID() == epochID );
    CATCH_REQUIRE( deserialized->getTransactionList()->size() == 1 );
    CATCH_REQUIRE( deserialized->getSignature() == signature );

    // Verify transaction data is preserved
    auto deserializedTxs = deserialized->getTransactionList()->getItems();
    CATCH_REQUIRE( *deserializedTxs->at( 0 )->getData() == *txVec->at( 0 )->getData() );

    // Verify re-serialization matches
    auto reserialized = deserialized->serializeProposal();
    CATCH_REQUIRE( *reserialized == *serialized );
}

CATCH_TEST_CASE(
    "BiteBlockProposalSerializer sanity check fails on corrupt data", "[bite][serializer][proposal][error]" ) {
    // Create corrupt data that is not a valid FlatBuffer
    auto corruptData = make_shared< vector< uint8_t > >( 100, 0xFF );

    CATCH_REQUIRE_THROWS( BiteBlockProposalSerializer::serializedSanityCheck( corruptData ) );
}

CATCH_TEST_CASE(
    "BiteBlockProposalSerializer deserialize fails on corrupt FlatBuffer", "[bite][serializer][proposal][error]" ) {
    ConsensusEngine engine( 0, 100000000 );
    std::shared_ptr< Schain > chain;
    std::shared_ptr< Node > node;
    auto cryptoManager = createTestCryptoManager( chain, node, engine );

    // Create corrupt data
    auto corruptData = make_shared< vector< uint8_t > >( 100, 0xFF );

    CATCH_REQUIRE_THROWS( BiteBlockProposalSerializer::deserialize( corruptData, cryptoManager, false ) );
}

CATCH_TEST_CASE(
    "BiteBlockProposalSerializer with multiple transactions", "[bite][serializer][proposal]" ) {
    // Setup environment
    ConsensusEngine engine( 0, 100000000 );
    std::shared_ptr< Schain > chain;
    std::shared_ptr< Node > node;
    auto cryptoManager = createTestCryptoManager( chain, node, engine );

    // Setup Proposal Data
    schain_id schainID = 1;
    node_id proposerNodeID = 1;
    block_id blockID = 300;
    epoch_id epochID = 30;
    schain_index proposerIndex = 1;
    u256 stateRoot = 0xABCDEF;
    uint64_t timeStamp = std::time( nullptr );
    uint32_t timeStampMs = 789;
    string signature = "multi_tx_sig";

    // Create multiple Transactions
    auto keys = generateKeys( 1, 1 );
    auto tx1 = buildBite1Transaction( vector< uint8_t >{ 0x01, 0x02 }, vector< uint8_t >( 20, 0x11 ),
        static_cast< uint64_t >( epochID ), keys.commonPublic );
    auto tx2 = buildBite1Transaction( vector< uint8_t >{ 0x03, 0x04 }, vector< uint8_t >( 20, 0x22 ),
        static_cast< uint64_t >( epochID ), keys.commonPublic );
    auto tx3 = buildBite1Transaction( vector< uint8_t >{ 0x05, 0x06 }, vector< uint8_t >( 20, 0x33 ),
        static_cast< uint64_t >( epochID ), keys.commonPublic );

    auto txVec = make_shared< vector< ptr< Transaction > > >();
    txVec->push_back( tx1 );
    txVec->push_back( tx2 );
    txVec->push_back( tx3 );
    auto txList = make_shared< TransactionList >( txVec );

    auto proposal = BlockProposal::makeFromSerialized(
        schainID, proposerNodeID, blockID, epochID, proposerIndex, txList, stateRoot, timeStamp,
        timeStampMs, signature, nullptr
    );

    auto header = make_shared< BlockProposalHeader >( *proposal );

    // Serialize
    auto serialized = BiteBlockProposalSerializer::serializeTransactionsAndCompleteSerialization(
        header, txList );
    CATCH_REQUIRE( serialized != nullptr );

    // Sanity check
    CATCH_REQUIRE_NOTHROW( BiteBlockProposalSerializer::serializedSanityCheck( serialized ) );

    // Deserialize
    auto deserialized = BiteBlockProposalSerializer::deserialize( serialized, cryptoManager, false );
    CATCH_REQUIRE( deserialized != nullptr );
    CATCH_REQUIRE( deserialized->getTransactionList()->size() == 3 );

    // Verify individual transaction data is preserved
    auto deserializedTxs = deserialized->getTransactionList()->getItems();
    CATCH_REQUIRE( *deserializedTxs->at( 0 )->getData() == *txVec->at( 0 )->getData() );
    CATCH_REQUIRE( *deserializedTxs->at( 1 )->getData() == *txVec->at( 1 )->getData() );
    CATCH_REQUIRE( *deserializedTxs->at( 2 )->getData() == *txVec->at( 2 )->getData() );

    // Roundtrip check
    auto reserialized = deserialized->serializeProposal();
    CATCH_REQUIRE( *reserialized == *serialized );
}

#endif