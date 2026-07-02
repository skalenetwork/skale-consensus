#include "thirdparty/catch.hpp"

#ifdef BITE

#include "bite/BiteCommittedBlockSerializer.h"
#include "headers/CommittedBlockHeader.h"
#include "datastructures/CommittedBlock.h"
#include "datastructures/TransactionList.h"
#include "crypto/DecryptedAESKeyList.h"

#include "BiteTestUtils.h"
#include "libBLS/test/utils.h"

using namespace std;
using namespace BiteTestUtils;

CATCH_TEST_CASE( "BiteCommittedBlockSerializer serialize and deserialize", "[bite][serializer][committed]" ) {
    ConsensusEngine engine( 0, 100000000 );
    std::shared_ptr< Schain > chain;
    std::shared_ptr< Node > node;
    auto cryptoManager = createTestCryptoManager( chain, node, engine );
    auto biteManager = createTestBiteManager( chain );

    // Setup Block Data
    schain_id schainID = 1;
    node_id proposerNodeID = 1;
    block_id blockID = 100;
    epoch_id epochID = 10;
    schain_index proposerIndex = 1;
    u256 stateRoot = 0xabcdef;
    uint64_t timeStamp = std::time( nullptr );
    uint32_t timeStampMs = 123;
    string signature = "mock_sig";
    string thresholdSig = "mock_threshold_sig";
    string daSig = "mock_da_sig";

    // Create plain (non-BITE) transactions for serialization test
    auto plainTx1 = EthTransactionEncoder::generateSampleTx();
    plainTx1->to = std::vector< uint8_t >( ADDRESS_SIZE, 0x11 );  // non-BITE address
    plainTx1->data = { 0x01, 0x02 };
    auto tx1 = std::make_shared< Transaction >(
        EthTransactionEncoder::signAndEncodeTx( plainTx1 ), false );

    auto plainTx2 = EthTransactionEncoder::generateSampleTx();
    plainTx2->to = std::vector< uint8_t >( ADDRESS_SIZE, 0x22 );  // non-BITE address
    plainTx2->data = { 0x03, 0x04 };
    auto tx2 = std::make_shared< Transaction >(
        EthTransactionEncoder::signAndEncodeTx( plainTx2 ), false );

    auto txVec = make_shared< vector< ptr< Transaction > > >();
    txVec->push_back( tx1 );
    txVec->push_back( tx2 );
    auto txList = make_shared< TransactionList >( txVec );

    // Empty AES Keys - plain transactions don't need them
    auto aesKeys = make_shared< DecryptedAESKeyList >();

    auto block = CommittedBlock::make( schainID, proposerNodeID, blockID, epochID, proposerIndex,
        txList, stateRoot, timeStamp, timeStampMs, signature, thresholdSig,
        std::nullopt,
        daSig, aesKeys,
        DecryptedTransactions()
    );

    auto header = make_shared< CommittedBlockHeader >( *block );

    // 1. Test Serialization
    ptr< vector< uint8_t > > serialized = nullptr;
    CATCH_REQUIRE_NOTHROW(
        serialized = BiteCommittedBlockSerializer::serializeTransactionsAndCompleteSerialization(
            *header, *txList, *aesKeys ) );

    CATCH_REQUIRE( serialized != nullptr );
    CATCH_REQUIRE( serialized->size() > 0 );

    // 2. Test Sanity Check
    CATCH_REQUIRE_NOTHROW( BiteCommittedBlockSerializer::serializedSanityCheck( serialized ) );

    // 3. Test Deserialization
    ptr< CommittedBlock > deserialized = nullptr;
    bool verifySig = false;

    CATCH_REQUIRE_NOTHROW( deserialized = BiteCommittedBlockSerializer::deserialize(
                               serialized, cryptoManager, biteManager, verifySig ) );

    CATCH_REQUIRE( deserialized != nullptr );
    CATCH_REQUIRE( deserialized->getBlockID() == blockID );
    CATCH_REQUIRE( deserialized->getEpochID() == epochID );
    CATCH_REQUIRE( deserialized->getTransactionList()->size() == 2 );

    // Verify transaction data is preserved
    auto deserializedTxs = deserialized->getTransactionList()->getItems();
    CATCH_REQUIRE( *deserializedTxs->at( 0 )->getData() == *txVec->at( 0 )->getData() );
    CATCH_REQUIRE( *deserializedTxs->at( 1 )->getData() == *txVec->at( 1 )->getData() );

    // Verify roundtrip
    auto reserialized = deserialized->serialize();
    CATCH_REQUIRE( reserialized->size() == serialized->size() );
    CATCH_REQUIRE( *reserialized == *serialized );
}

CATCH_TEST_CASE(
    "BiteCommittedBlockSerializer sanity check fails on corrupt data", "[bite][serializer][committed][error]" ) {
    // Create corrupt data that is not a valid FlatBuffer
    auto corruptData = make_shared< vector< uint8_t > >( 100, 0xFF );

    CATCH_REQUIRE_THROWS( BiteCommittedBlockSerializer::serializedSanityCheck( corruptData ) );
}

CATCH_TEST_CASE(
    "BiteCommittedBlockSerializer deserialize fails on corrupt FlatBuffer", "[bite][serializer][committed][error]" ) {
    ConsensusEngine engine( 0, 100000000 );
    std::shared_ptr< Schain > chain;
    std::shared_ptr< Node > node;
    auto cryptoManager = createTestCryptoManager( chain, node, engine );
    auto biteManager = createTestBiteManager( chain );

    // Create corrupt data
    auto corruptData = make_shared< vector< uint8_t > >( 100, 0xFF );

    CATCH_REQUIRE_THROWS( BiteCommittedBlockSerializer::deserialize(
        corruptData, cryptoManager, biteManager, false ) );
}

CATCH_TEST_CASE(
    "BiteCommittedBlockSerializer with multiple plain transactions", "[bite][serializer][committed]" ) {
    ConsensusEngine engine( 0, 100000000 );
    std::shared_ptr< Schain > chain;
    std::shared_ptr< Node > node;
    auto cryptoManager = createTestCryptoManager( chain, node, engine );
    auto biteManager = createTestBiteManager( chain );

    // Setup Block Data
    schain_id schainID = 1;
    node_id proposerNodeID = 1;
    block_id blockID = 200;
    epoch_id epochID = 20;
    schain_index proposerIndex = 1;
    u256 stateRoot = 0x123456;
    uint64_t timeStamp = std::time( nullptr );
    uint32_t timeStampMs = 456;
    string signature = "sig_multi_plain";
    string thresholdSig = "threshold_sig_multi";
    string daSig = "da_sig_multi";

    // Create multiple plain transactions
    auto plainTx1 = EthTransactionEncoder::generateSampleTx();
    plainTx1->to = std::vector< uint8_t >( ADDRESS_SIZE, 0x33 );
    plainTx1->data = { 0xAA, 0xBB };
    auto tx1 = std::make_shared< Transaction >(
        EthTransactionEncoder::signAndEncodeTx( plainTx1 ), false );

    auto plainTx2 = EthTransactionEncoder::generateSampleTx();
    plainTx2->to = std::vector< uint8_t >( ADDRESS_SIZE, 0x44 );
    plainTx2->data = { 0xCC, 0xDD };
    auto tx2 = std::make_shared< Transaction >(
        EthTransactionEncoder::signAndEncodeTx( plainTx2 ), false );

    auto plainTx3 = EthTransactionEncoder::generateSampleTx();
    plainTx3->to = std::vector< uint8_t >( ADDRESS_SIZE, 0x55 );
    plainTx3->data = { 0xEE, 0xFF };
    auto tx3 = std::make_shared< Transaction >(
        EthTransactionEncoder::signAndEncodeTx( plainTx3 ), false );

    auto txVec = make_shared< vector< ptr< Transaction > > >();
    txVec->push_back( tx1 );
    txVec->push_back( tx2 );
    txVec->push_back( tx3 );
    auto txList = make_shared< TransactionList >( txVec );

    // Empty AES Keys - plain transactions don't need them
    auto aesKeys = make_shared< DecryptedAESKeyList >();

    auto block = CommittedBlock::make( schainID, proposerNodeID, blockID, epochID, proposerIndex,
        txList, stateRoot, timeStamp, timeStampMs, signature, thresholdSig,
        std::nullopt,
        daSig, aesKeys,
        DecryptedTransactions()
    );

    auto header = make_shared< CommittedBlockHeader >( *block );

    // Serialize
    auto serialized = BiteCommittedBlockSerializer::serializeTransactionsAndCompleteSerialization(
        *header, *txList, *aesKeys );
    CATCH_REQUIRE( serialized != nullptr );

    // Sanity check
    CATCH_REQUIRE_NOTHROW( BiteCommittedBlockSerializer::serializedSanityCheck( serialized ) );

    // Deserialize
    auto deserialized = BiteCommittedBlockSerializer::deserialize(
        serialized, cryptoManager, biteManager, false );
    CATCH_REQUIRE( deserialized != nullptr );
    CATCH_REQUIRE( deserialized->getBlockID() == blockID );
    CATCH_REQUIRE( deserialized->getTransactionList()->size() == 3 );

    // Verify individual transaction data is preserved
    auto deserializedTxs = deserialized->getTransactionList()->getItems();
    CATCH_REQUIRE( *deserializedTxs->at( 0 )->getData() == *txVec->at( 0 )->getData() );
    CATCH_REQUIRE( *deserializedTxs->at( 1 )->getData() == *txVec->at( 1 )->getData() );
    CATCH_REQUIRE( *deserializedTxs->at( 2 )->getData() == *txVec->at( 2 )->getData() );

    // Roundtrip check
    auto reserialized = deserialized->serialize();
    CATCH_REQUIRE( *reserialized == *serialized );
}

CATCH_TEST_CASE(
    "BiteCommittedBlockSerializer with reencryption threshold signature", "[bite][serializer][committed][bite2]" ) {
    ConsensusEngine engine( 0, 100000000 );
    std::shared_ptr< Schain > chain;
    std::shared_ptr< Node > node;
    auto cryptoManager = createTestCryptoManager( chain, node, engine );
    auto biteManager = createTestBiteManager( chain );

    // Setup Block Data
    schain_id schainID = 1;
    node_id proposerNodeID = 1;
    block_id blockID = 300;
    epoch_id epochID = 30;
    schain_index proposerIndex = 1;
    u256 stateRoot = 0x789abc;
    uint64_t timeStamp = std::time( nullptr );
    uint32_t timeStampMs = 789;
    string signature = "mock_sig_bite2";
    string thresholdSig = "mock_threshold_sig_bite2";
    string reencryptionThresholdSig = "mock_reencryption_threshold_sig_bite2";
    string daSig = "mock_da_sig_bite2";

    // Create plain transactions
    auto plainTx1 = EthTransactionEncoder::generateSampleTx();
    plainTx1->to = std::vector< uint8_t >( ADDRESS_SIZE, 0x77 );
    plainTx1->data = { 0xAA, 0xBB };
    auto tx1 = std::make_shared< Transaction >(
        EthTransactionEncoder::signAndEncodeTx( plainTx1 ), false );

    auto txVec = make_shared< vector< ptr< Transaction > > >();
    txVec->push_back( tx1 );
    auto txList = make_shared< TransactionList >( txVec );

    auto aesKeys = make_shared< DecryptedAESKeyList >();

    auto block = CommittedBlock::make( schainID, proposerNodeID, blockID, epochID, proposerIndex,
        txList, stateRoot, timeStamp, timeStampMs, signature, thresholdSig,
        std::optional<string>(reencryptionThresholdSig),
        daSig, aesKeys,
        DecryptedTransactions()
    );

    auto header = make_shared< CommittedBlockHeader >( *block );

    // 1. Test Serialization
    auto serialized = BiteCommittedBlockSerializer::serializeTransactionsAndCompleteSerialization(
        *header, *txList, *aesKeys );
    CATCH_REQUIRE( serialized != nullptr );
    CATCH_REQUIRE( serialized->size() > 0 );

    // 2. Test Sanity Check
    CATCH_REQUIRE_NOTHROW( BiteCommittedBlockSerializer::serializedSanityCheck( serialized ) );

    // 3. Test Deserialization
    auto deserialized = BiteCommittedBlockSerializer::deserialize(
        serialized, cryptoManager, biteManager, false );
    CATCH_REQUIRE( deserialized != nullptr );
    CATCH_REQUIRE( deserialized->getBlockID() == blockID );

    // 4. Verify reencryption threshold signature is preserved
    auto deserializedReencryptionSig = deserialized->getReencryptionThresholdSig();
    CATCH_REQUIRE( deserializedReencryptionSig.has_value() );
    CATCH_REQUIRE( deserializedReencryptionSig.value() == reencryptionThresholdSig );

    // 5. Verify roundtrip
    auto reserialized = deserialized->serialize();
    CATCH_REQUIRE( *reserialized == *serialized );
}

CATCH_TEST_CASE(
    "BiteCommittedBlockSerializer without reencryption threshold signature", "[bite][serializer][committed][bite2]" ) {
    ConsensusEngine engine( 0, 100000000 );
    std::shared_ptr< Schain > chain;
    std::shared_ptr< Node > node;
    auto cryptoManager = createTestCryptoManager( chain, node, engine );
    auto biteManager = createTestBiteManager( chain );

    // Setup Block Data
    schain_id schainID = 1;
    node_id proposerNodeID = 1;
    block_id blockID = 400;
    epoch_id epochID = 40;
    schain_index proposerIndex = 1;
    u256 stateRoot = 0xdef123;
    uint64_t timeStamp = std::time( nullptr );
    uint32_t timeStampMs = 999;
    string signature = "mock_sig_no_reencryption";
    string thresholdSig = "mock_threshold_sig_no_reencryption";
    string daSig = "mock_da_sig_no_reencryption";

    // Create plain transactions
    auto plainTx1 = EthTransactionEncoder::generateSampleTx();
    plainTx1->to = std::vector< uint8_t >( ADDRESS_SIZE, 0x88 );
    plainTx1->data = { 0xCC, 0xDD };
    auto tx1 = std::make_shared< Transaction >(
        EthTransactionEncoder::signAndEncodeTx( plainTx1 ), false );

    auto txVec = make_shared< vector< ptr< Transaction > > >();
    txVec->push_back( tx1 );
    auto txList = make_shared< TransactionList >( txVec );

    auto aesKeys = make_shared< DecryptedAESKeyList >();

    auto block = CommittedBlock::make( schainID, proposerNodeID, blockID, epochID, proposerIndex,
        txList, stateRoot, timeStamp, timeStampMs, signature, thresholdSig,
        std::nullopt,  // No reencryption threshold signature
        daSig, aesKeys,
        DecryptedTransactions()
    );

    auto header = make_shared< CommittedBlockHeader >( *block );

    // 1. Test Serialization
    auto serialized = BiteCommittedBlockSerializer::serializeTransactionsAndCompleteSerialization(
        *header, *txList, *aesKeys );
    CATCH_REQUIRE( serialized != nullptr );
    CATCH_REQUIRE( serialized->size() > 0 );

    // 2. Test Sanity Check
    CATCH_REQUIRE_NOTHROW( BiteCommittedBlockSerializer::serializedSanityCheck( serialized ) );

    // 3. Test Deserialization
    auto deserialized = BiteCommittedBlockSerializer::deserialize(
        serialized, cryptoManager, biteManager, false );
    CATCH_REQUIRE( deserialized != nullptr );
    CATCH_REQUIRE( deserialized->getBlockID() == blockID );

    // 4. Verify reencryption threshold signature is absent
    auto deserializedReencryptionSig = deserialized->getReencryptionThresholdSig();
    CATCH_REQUIRE( !deserializedReencryptionSig.has_value() );

    // 5. Verify roundtrip
    auto reserialized = deserialized->serialize();
    CATCH_REQUIRE( *reserialized == *serialized );
}

#endif
