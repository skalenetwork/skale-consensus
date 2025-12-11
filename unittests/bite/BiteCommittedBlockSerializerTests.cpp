#include "thirdparty/catch.hpp"

#ifdef BITE

#include "bite/BiteCommittedBlockSerializer.h"
#include "bite/BiteManager.h"
#include "headers/CommittedBlockHeader.h"
#include "datastructures/CommittedBlock.h"
#include "datastructures/TransactionList.h"
#include "crypto/DecryptedAESKeyList.h"
#include "crypto/CryptoManager.h"
#include "chains/Schain.h"
#include "node/ConsensusEngine.h"
#include "node/Node.h"
#include "node/NodeInfo.h"
#include "thirdparty/json.hpp"
#include <ctime>

#include "BiteTestUtils.h"
#include "libBLS/test/utils.h"

using namespace std;
using namespace BiteTestUtils;

// Helper to create a valid CryptoManager with necessary dependencies
static std::shared_ptr< CryptoManager > createTestCryptoManager(
    std::shared_ptr< Schain >& chain_out, std::shared_ptr< Node >& node_out,
    ConsensusEngine& engine ) {
    nlohmann::json cfg;
    cfg["nodeID"] = 1;
    cfg["nodeName"] = "testNode";
    cfg["bindIP"] = "127.0.0.1";
    cfg["basePort"] = 10000;
    string gethUrl = "";
    string schainName = "testChain";
    schain_id schainId = 1337;

    node_out = std::make_shared< Node >( cfg, &engine, false, "", "", "", "", nullptr, "", nullptr,
        nullptr, gethUrl, nullptr, nullptr, nullptr, false );

    auto nodeInfo = std::make_shared< NodeInfo >( 1, "127.0.0.1", 10000, 1337, 1 );
    node_out->setNodeInfo( nodeInfo );

    chain_out = std::make_shared< Schain >( node_out, 1, schainId, nullptr, schainName );

    // node_out->setSchain(chain_out); // Avoid loop to prevent DB init crash/issues if any

    return std::make_shared< CryptoManager >( *chain_out );
}

CATCH_TEST_CASE( "BiteCommittedBlockSerializer functionality", "[bite][serializer]" ) {
    ConsensusEngine engine( 0, 100000000 );
    std::shared_ptr< Schain > chain;
    std::shared_ptr< Node > node;
    auto cryptoManager = createTestCryptoManager( chain, node, engine );
    auto biteManager = make_shared< BiteManager >( *chain );

    // Setup Block Data
    schain_id schainID = 1;
    node_id proposerNodeID = 1;
    block_id blockID = 100;
    epoch_id epochID = 10;
    schain_index proposerIndex = 1;
    u256 stateRoot = 0xabcdef;
    uint64_t timeStamp = std::time( nullptr );  // Use current time to satisfy MODERN_TIME check
    uint32_t timeStampMs = 123;
    string signature = "mock_sig";
    string thresholdSig = "mock_threshold_sig";
    string daSig = "mock_da_sig";

    // Create plain (non-BITE) transactions for serialization test
    // Plain transactions don't need crypto decryption during deserialization
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

    // Create Block Header
    // We need a CommittedBlock to create a header easily, or manually construct one if constructor
    // available CommittedBlockHeader constructor takes json or CommittedBlock. Let's create a
    // CommittedBlock first to get the header.

    auto block = CommittedBlock::make( schainID, proposerNodeID, blockID, epochID, proposerIndex,
        txList, stateRoot, timeStamp, timeStampMs, signature, thresholdSig, daSig, aesKeys,
        DecryptedTransactions()  // empty decrypted txs for now
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
    // Note: BiteCommittedBlockSerializer::deserialize requires BiteManager to verify/decrypt,
    // which might fail if we don't have real keys set up in BiteManager/CryptoManager.
    // However, we can basic check if it constructs the block.
    // If BiteManager tries to verify real crypto, it might fail.
    // We can use a mock BiteManager or configure it to skip real crypto if possible?
    // BiteManager constructor takes Schain.
    // BiteManager::verifyAndDecryptTransactionList uses BiteEngine.
    // If we passed real crypto flag?
    // Let's assume for serialization unit test we might hit issues if it tries real decryption.
    // But verifyAndDecryptTransactionList runs automatically on deserialize.

    // To make this pass without complex setup, we might need to rely on the fact
    // that deserialize calls _biteManager->verifyAndDecryptTransactionList.
    // If we want to test PURE serialization, we verified it produces a buffer.

    // Let's attempt deserialization. If it throws due to crypto, we'll verify it parses at least.

    bool verifySig = false;  // skip sig verify to avoid crypto manager errors

    CATCH_REQUIRE_NOTHROW( deserialized = BiteCommittedBlockSerializer::deserialize(
                               serialized, cryptoManager, biteManager, verifySig ) );

    CATCH_REQUIRE( deserialized != nullptr );
    CATCH_REQUIRE( deserialized->getBlockID() == blockID );
    CATCH_REQUIRE( deserialized->getEpochID() == epochID );
    CATCH_REQUIRE( deserialized->getTransactionList()->size() == 2 );

    // Verify AES keys were deserialized (accessible via friend/getter if available, or just trust
    // correct block creation) CommittedBlock doesn't expose getDecryptedAesKeyList publically
    // easily directly? It does! block->decryptedAesKeyList is private but we passed it to
    // constructor. Wait, CommittedBlock has no getter for aesKeyList? I see in CommittedBlock.cpp:
    // DecryptedAESKeyList _aesKeyList ... -> this->decryptedAesKeyList = _aesKeyList
    // But no getter in header file probably?
    // Actually `BiteCommittedBlockSerializer.cpp` does `BiteAESKeySerializer::deserialize` and
    // passes it.

    // We can indirectly check by re-serializing?
    auto reserialized = deserialized->serialize();
    CATCH_REQUIRE( reserialized->size() == serialized->size() );
    CATCH_REQUIRE( *reserialized == *serialized );
}

#endif
