#include "thirdparty/catch.hpp"

#ifdef BITE

// BiteTestUtils includes SkaleCommon.h, BiteManager.h, CryptoManager.h and other dependencies
#include "BiteTestUtils.h"
#include "libBLS/test/utils.h"

#include "crypto/AESKeyDecryptionShareList.h"
#include "crypto/ConsensusAESKeyDecryptionShare.h"
#include "bite/BiteAESDecryptionShareSerializer.h"
#include "flatb/decryption_shares_generated.h"
#include <flatbuffers/flatbuffers.h>

using namespace std;
using namespace BiteTestUtils;

CATCH_TEST_CASE(
    "BiteAESDecryptionShareSerializer serialize with single share per tx (BITE1)",
    "[bite][serialize][decryptionshare][backward]" ) {
    
    ConsensusEngine engine( 0, 100000000 );
    std::shared_ptr< Schain > chain;
    std::shared_ptr< Node > node;
    createTestCryptoManager( chain, node, engine );  // Sets up chain/node
    auto biteManager = createTestBiteManager( chain );

    // BITE1 layout: one ciphertext per transaction
    block_id blockId = 100;
    schain_index proposerIndex = 1;
    schain_index decryptorIndex = chain->getSchainIndex();

    auto shareList = make_shared< AESKeyDecryptionShareList >( blockId, proposerIndex, decryptorIndex );

    // Add single share for tx 0 and tx 1
    auto shares0 = biteManager->createAESDecryptionShares( "share_data_0", decryptorIndex, false );
    shareList->addShares( 0, shares0 );

    auto shares1 = biteManager->createAESDecryptionShares( "share_data_1", decryptorIndex, false );
    shareList->addShares( 1, shares1 );

    CATCH_REQUIRE( shareList->size() == 2 );

    // Serialize
    auto serialized = BiteAESDecryptionShareSerializer::serialize( shareList );
    CATCH_REQUIRE( serialized != nullptr );
    CATCH_REQUIRE( serialized->size() > 0 );

    // Sanity check
    CATCH_REQUIRE_NOTHROW( BiteAESDecryptionShareSerializer::serializedSanityCheck( serialized ) );

    // Verify structure by parsing FlatBuffer directly
    auto fbShares = skale_fb::GetDecryptionShares( serialized->data() );
    CATCH_REQUIRE( fbShares != nullptr );
    CATCH_REQUIRE( fbShares->block_id() == ( uint64_t ) blockId );
    CATCH_REQUIRE( fbShares->proposer_index() == ( uint64_t ) proposerIndex );
    CATCH_REQUIRE( fbShares->decryptor_index() == ( uint64_t ) decryptorIndex );
    CATCH_REQUIRE( fbShares->decryption_shares() != nullptr );
    CATCH_REQUIRE( fbShares->decryption_shares()->size() == 2 );
}

CATCH_TEST_CASE(
    "BiteAESDecryptionShareSerializer serialize with multiple shares per tx (BITE2)",
    "[bite][serialize][decryptionshare][bite2]" ) {
    // BITE2 layout: multiple ciphertexts per transaction (CAT)
    ConsensusEngine engine( 0, 100000000 );
    std::shared_ptr< Schain > chain;
    std::shared_ptr< Node > node;
    createTestCryptoManager( chain, node, engine );  // Sets up chain/node
    auto biteManager = createTestBiteManager( chain );

    block_id blockId = 200;
    schain_index proposerIndex = 1;
    schain_index decryptorIndex = chain->getSchainIndex();

    auto shareList = make_shared< AESKeyDecryptionShareList >( blockId, proposerIndex, decryptorIndex );

    // Add 3 shares for tx 0 (CAT with 3 encrypted args) - comma-separated
    auto shares0 = biteManager->createAESDecryptionShares( "share0_0,share0_1,share0_2", decryptorIndex, false );
    shareList->addShares( 0, shares0 );

    // Add 2 shares for tx 1 (CAT with 2 encrypted args)
    auto shares1 = biteManager->createAESDecryptionShares( "share1_0,share1_1", decryptorIndex, false );
    shareList->addShares( 1, shares1 );

    // Add 1 share for tx 2 (regular BITE1 tx)
    auto shares2 = biteManager->createAESDecryptionShares( "share2_0", decryptorIndex, false );
    shareList->addShares( 2, shares2 );

    // Verify counts
    CATCH_REQUIRE( shareList->size() == 3 );

    // Serialize
    auto serialized = BiteAESDecryptionShareSerializer::serialize( shareList );
    CATCH_REQUIRE( serialized != nullptr );
    CATCH_REQUIRE( serialized->size() > 0 );

    // Sanity check
    CATCH_REQUIRE_NOTHROW( BiteAESDecryptionShareSerializer::serializedSanityCheck( serialized ) );

    // Verify structure
    auto fbShares = skale_fb::GetDecryptionShares( serialized->data() );
    CATCH_REQUIRE( fbShares != nullptr );
    CATCH_REQUIRE( fbShares->decryption_shares()->size() == 3 );  // 3 transactions
}

CATCH_TEST_CASE(
    "BiteAESDecryptionShareSerializer sanity check fails on corrupt data",
    "[bite][serialize][decryptionshare][error]" ) {
    auto corruptData = make_shared< vector< uint8_t > >( 100, 0xFF );

    CATCH_REQUIRE_THROWS( BiteAESDecryptionShareSerializer::serializedSanityCheck( corruptData ) );
}

CATCH_TEST_CASE(
    "BiteAESDecryptionShareSerializer roundtrip preserves data",
    "[bite][serialize][decryptionshare][component]" ) {
    // This test does full serialize -> deserialize roundtrip
    ConsensusEngine engine( 0, 100000000 );
    std::shared_ptr< Schain > chain;
    std::shared_ptr< Node > node;
    auto cryptoManager = createTestCryptoManager( chain, node, engine );
    auto biteManager = createTestBiteManager( chain );

    block_id blockId = 300;
    schain_index proposerIndex = 1;
    schain_index decryptorIndex = chain->getSchainIndex();

    auto shareList = make_shared< AESKeyDecryptionShareList >( blockId, proposerIndex, decryptorIndex );

    // Create shares using the real infrastructure
    auto shares0 = biteManager->createAESDecryptionShares( "mock_share_data_0", decryptorIndex, false );
    shareList->addShares( 0, shares0 );

    auto shares1 = biteManager->createAESDecryptionShares( "mock_share_data_1", decryptorIndex, false );
    shareList->addShares( 1, shares1 );

    // Serialize
    auto serialized = BiteAESDecryptionShareSerializer::serialize( shareList );
    CATCH_REQUIRE( serialized != nullptr );

    // Sanity check
    CATCH_REQUIRE_NOTHROW( BiteAESDecryptionShareSerializer::serializedSanityCheck( serialized ) );

    // Deserialize
    auto deserialized = BiteAESDecryptionShareSerializer::deserialize(
        serialized, cryptoManager, CryptographicValidationMode::SkipValidationTrustedSource );
    CATCH_REQUIRE( deserialized != nullptr );
    CATCH_REQUIRE( deserialized->size() == 2 );
    CATCH_REQUIRE( deserialized->getBlockId() == blockId );
    CATCH_REQUIRE( deserialized->getProposerIndex() == proposerIndex );
    CATCH_REQUIRE( deserialized->getDecryptorIndex() == decryptorIndex );
}

#endif

