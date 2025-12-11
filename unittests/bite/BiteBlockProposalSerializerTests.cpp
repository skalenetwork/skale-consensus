#include "thirdparty/catch.hpp"

#ifdef BITE

#include "bite/BiteBlockProposalSerializer.h"
#include "bite/BiteManager.h"
#include "headers/BlockProposalHeader.h"
#include "datastructures/BlockProposal.h"
#include "datastructures/TransactionList.h"
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

    return std::make_shared< CryptoManager >( *chain_out );
}

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
    auto tx1 = buildBite1Transaction( vector< uint8_t >{ 0x01 }, vector< uint8_t >( 20, 0x11 ),
        static_cast< uint64_t >( epochID ), keys.commonPublic );
    auto txVec = make_shared< vector< ptr< Transaction > > >();
    txVec->push_back( tx1 );
    auto txList = make_shared< TransactionList >( txVec );

    // Create Proposal
    // Using simple constructor if possible, or makeFromSerialized/similar pattern
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

    // Verify re-serialization matches
    auto reserialized = deserialized->serializeProposal();
    CATCH_REQUIRE( *reserialized == *serialized );
}

#endif