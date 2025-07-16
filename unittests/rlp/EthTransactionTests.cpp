//
// EthTransaction Unit Tests  
// Tests for EthTransaction functionality and security
//

#include "thirdparty/catch.hpp"
#include "rlp/EthTransaction.h"
#include "rlp/RLPStream.h"
#include <SkaleCommon.h>
#include <vector>
#include <array>

// Test helper functions
namespace {
    // Sample private key for testing (DO NOT USE IN PRODUCTION)
    std::vector<uint8_t> getTestPrivateKey() {
        return {
            0x47, 0x10, 0xd4, 0x67, 0xa3, 0x0c, 0x65, 0x4b,
            0x6b, 0xf5, 0xfb, 0xef, 0xf4, 0x11, 0x87, 0x1b,
            0x3d, 0x56, 0x79, 0x1c, 0x8f, 0x3a, 0x4b, 0xd4,
            0x60, 0x54, 0x3b, 0xaf, 0x63, 0xeb, 0x47, 0x19
        };
    }

    // Sample address (20 bytes)
    std::vector<uint8_t> getTestAddress() {
        return {
            0xd4, 0x6e, 0x8d, 0xd6, 0x7c, 0x5d, 0x32, 0xbe,
            0x8d, 0x46, 0xe8, 0xdd, 0x67, 0xc5, 0xd3, 0x2b,
            0xe8, 0x05, 0x8b, 0xb8
        };
    }

    LegacyTx legacyTxSample(
        RLPStream::u256toBytes(1), 
        RLPStream::u256toBytes(21000), 
        getTestAddress(), 
        RLPStream::u256toBytes(1000000000000000000ULL), 
        {}, 
        { 0x01 }, 
        RLPStream::u256toBytes(20000000000ULL)
    );

    Type1Tx type1TxSample(
        RLPStream::u256toBytes(2), 
        RLPStream::u256toBytes(30000), 
        getTestAddress(), 
        RLPStream::u256toBytes(500000000000000000ULL), 
        {}, 
        RLPStream::u256toBytes(1), 
        RLPStream::u256toBytes(25000000000ULL), 
        {}
    );

    Type2Tx type2TxSample(
        RLPStream::u256toBytes(3), 
        RLPStream::u256toBytes(25000), 
        getTestAddress(), 
        RLPStream::u256toBytes(250000000000000000ULL), 
        {}, 
        RLPStream::u256toBytes(1), 
        RLPStream::u256toBytes(2000000000ULL), 
        RLPStream::u256toBytes(30000000000ULL), 
        {}
    );




}

// ===================== BASIC TRANSACTION CREATION TESTS =====================

TEST_CASE("LegacyTx creation and basic properties", "[eth-transaction][unit][correctness]") {
    uint256 nonce = RLPStream::u256toBytes(1);
    uint256 gasLimit = RLPStream::u256toBytes(21000);
    auto to = getTestAddress();
    uint256 value = RLPStream::u256toBytes(1000000000); // 1 Gwei
    std::vector<uint8_t> data = {0x01, 0x02, 0x03};
    uint256 chainId = RLPStream::u256toBytes(1); // Ethereum mainnet
    uint256 gasPrice = RLPStream::u256toBytes(20000000); // 20 Gwei

    LegacyTx tx(nonce, gasLimit, to, value, data, chainId, gasPrice);

    REQUIRE(tx.nonce == nonce);
    REQUIRE(tx.gasLimit == gasLimit);
    REQUIRE(tx.to == to);
    REQUIRE(tx.value == value);
    REQUIRE(tx.data == data);
    REQUIRE(tx.chainId == chainId);
    REQUIRE(tx.gasPrice == gasPrice);
    REQUIRE(tx.getBytePrefix() == TxPrefix::NONE);
}

TEST_CASE("Type1Tx creation and basic properties", "[eth-transaction][unit][correctness]") {
    uint256 nonce = RLPStream::u256toBytes(2);
    uint256 gasLimit = RLPStream::u256toBytes(30000);
    auto to = getTestAddress();
    uint256 value = RLPStream::u256toBytes(500000000); // 0.5 Gwei
    std::vector<uint8_t> data = {0x04, 0x05, 0x06};
    uint256 chainId = RLPStream::u256toBytes(1);
    uint256 gasPrice = RLPStream::u256toBytes(25000000); // 25 Gwei
    std::vector<AccessTuple> accessList;

    Type1Tx tx(nonce, gasLimit, to, value, data, chainId, gasPrice, accessList);

    REQUIRE(tx.nonce == nonce);
    REQUIRE(tx.gasLimit == gasLimit);
    REQUIRE(tx.to == to);
    REQUIRE(tx.value == value);
    REQUIRE(tx.data == data);
    REQUIRE(tx.chainId == chainId);
    REQUIRE(tx.gasPrice == gasPrice);
    REQUIRE(tx.accessList.empty());
    REQUIRE(tx.getBytePrefix() == TxPrefix::TYPE1);
}

TEST_CASE("Type2Tx creation and basic properties", "[eth-transaction][unit][correctness]") {
    uint256 nonce = RLPStream::u256toBytes(3);
    uint256 gasLimit = RLPStream::u256toBytes(25000);
    auto to = getTestAddress();
    uint256 value = RLPStream::u256toBytes(250000000); // 0.25 Gwei
    std::vector<uint8_t> data = {0x07, 0x08, 0x09};
    uint256 chainId = RLPStream::u256toBytes(1);
    uint256 maxPriorityFeePerGas = RLPStream::u256toBytes(2000000); // 2 Gwei
    uint256 maxFeePerGas = RLPStream::u256toBytes(30000000); // 30 Gwei
    std::vector<AccessTuple> accessList;

    Type2Tx tx(nonce, gasLimit, to, value, data, chainId, maxPriorityFeePerGas, maxFeePerGas, accessList);

    REQUIRE(tx.nonce == nonce);
    REQUIRE(tx.gasLimit == gasLimit);
    REQUIRE(tx.to == to);
    REQUIRE(tx.value == value);
    REQUIRE(tx.data == data);
    REQUIRE(tx.chainId == chainId);
    REQUIRE(tx.maxPriorityFeePerGas == maxPriorityFeePerGas);
    REQUIRE(tx.maxFeePerGas == maxFeePerGas);
    REQUIRE(tx.accessList.empty());
    REQUIRE(tx.getBytePrefix() == TxPrefix::TYPE2);
}

// ===================== TRANSACTION SIGNING TESTS =====================

TEST_CASE("LegacyTx signing and verification", "[rlp][eth-transaction][unit][correctness]") {

    LegacyTx tx = legacyTxSample;
    auto privateKey = getTestPrivateKey();
    Signature sig = tx.sign(privateKey);

    // Verify signature is non-zero
    REQUIRE(sig.r != 0);
    REQUIRE(sig.s != 0);
    REQUIRE(sig.v != 0);

#ifdef MIRAGE
    // For post-EIP155 transactions, v should be >= 37 (chainId * 2 + 35 + {0|1})
    REQUIRE(sig.v >= 37);
#else
    // For legacy transactions, v should be 27 or 28
    REQUIRE(sig.v == 27 || sig.v == 28);
#endif

    // Verify the signature
    REQUIRE_NOTHROW(tx.verifySignature(sig));
}

TEST_CASE("Type1Tx signing and verification", "[rlp][eth-transaction][unit][correctness]") {

    Type1Tx tx = type1TxSample;
    auto privateKey = getTestPrivateKey();
    Signature sig = tx.sign(privateKey);

    // Verify signature is non-zero
    REQUIRE(sig.r != 0);
    REQUIRE(sig.s != 0);
    // For Type1 transactions, v should be 0 or 1
    REQUIRE((sig.v == 0 || sig.v == 1));

    // Verify the signature
    REQUIRE_NOTHROW(tx.verifySignature(sig));
}

TEST_CASE("Type2Tx signing and verification", "[rlp][eth-transaction][unit][correctness]") {

    Type2Tx tx = type2TxSample;
    auto privateKey = getTestPrivateKey();
    Signature sig = tx.sign(privateKey);

    // Verify signature is non-zero
    REQUIRE(sig.r != 0);
    REQUIRE(sig.s != 0);
    // For Type2 transactions, v should be 0 or 1
    REQUIRE((sig.v == 0 || sig.v == 1));

    // Verify the signature
    REQUIRE_NOTHROW(tx.verifySignature(sig));
}

// ===================== PRE-EIP155 REJECTION TESTS =====================

TEST_CASE("Pre-EIP155 legacy transaction rejection", "[rlp][eth-transaction][unit][correctness]") {
    // Create a legacy transaction
    LegacyTx tx = legacyTxSample;

    // Create a signature that looks like pre-EIP155 (v = 27 or 28)
    Signature preEip155Sig;
    preEip155Sig.v = 27; // Pre-EIP155 signature
    preEip155Sig.r = u256("0x18515461264373351373200002665853028612451056578545711640558177340181847433846");
    preEip155Sig.s = u256("0x46948507304638947509940763649030358759909902576025900602547168820602576006531");

#ifdef MIRAGE
    REQUIRE_THROWS(tx.verifySignature(preEip155Sig));
#else
    // In non-MIRAGE mode, we allow v = 27 or 28 for backward compatibility
    // but this should still fail because the signature won't verify against our transaction
    REQUIRE_THROWS(tx.verifySignature(preEip155Sig));
#endif
}

TEST_CASE("Invalid signature domain validation", "[rlp][eth-transaction][unit][correctness]") {
    LegacyTx tx = legacyTxSample;

    // Test signature with r = 0 (invalid)
    {
        Signature invalidSig;
        invalidSig.v = 37;
        invalidSig.r = 0; // Invalid: r must be > 0
        invalidSig.s = u256("0x46948507304638947509940763649030358759909902576025900602547168820602576006531");
        
        REQUIRE_THROWS(tx.verifySignature(invalidSig));
    }

    // Test signature with s = 0 (invalid)
    {
        Signature invalidSig;
        invalidSig.v = 37;
        invalidSig.r = u256("0x18515461264373351373200002665853028612451056578545711640558177340181847433846");
        invalidSig.s = 0; // Invalid: s must be > 0
        
        REQUIRE_THROWS(tx.verifySignature(invalidSig));
    }

    // Test signature with r >= curve order (invalid)
    {
        Signature invalidSig;
        invalidSig.v = 37;
        invalidSig.r = u256("0xfffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141"); // >= curve order
        invalidSig.s = u256("0x46948507304638947509940763649030358759909902576025900602547168820602576006531");
        
        REQUIRE_THROWS(tx.verifySignature(invalidSig));
    }
}

// ===================== RLP ENCODING TESTS =====================

TEST_CASE("LegacyTx RLP encoding without signature", "[rlp][eth-transaction][unit][correctness]") {
    LegacyTx tx = legacyTxSample;

    auto encoded = tx.rlpEncode(std::nullopt);
    
    REQUIRE(!encoded.empty());

    RLPItem item(encoded);
    REQUIRE(item.isList());
    LegacyTx decodedTx(item);

    // encoded tx must be equal to original
    REQUIRE(decodedTx.nonce == tx.nonce);
    REQUIRE(decodedTx.gasLimit == tx.gasLimit);
    REQUIRE(decodedTx.to == tx.to);
    REQUIRE(decodedTx.value == tx.value);
    REQUIRE(decodedTx.data == tx.data);
    // we do not compare the chainId - it is only used when building / encoding the tx into RLP.
    // When decoding, we do not set chainId field.
    REQUIRE(decodedTx.gasPrice == tx.gasPrice);
    REQUIRE(decodedTx.getBytePrefix() == tx.getBytePrefix());
}

TEST_CASE("Type1Tx RLP encoding with prefix", "[rlp][eth-transaction][unit][correctness]") {
    Type1Tx tx = type1TxSample;

    auto encoded = tx.rlpEncode(std::nullopt);
    REQUIRE(!encoded.empty());

    // Type1 transactions should have 0x01 prefix
    REQUIRE(encoded[0] == 0x01);

    // remove the prefix for RLPItem parsing
    encoded.erase(encoded.begin());

    // decode
    RLPItem item(encoded);
    REQUIRE(item.isList());
    Type1Tx decodedTx(item);

    // encoded tx must be equal to original
    REQUIRE(decodedTx.nonce == tx.nonce);
    REQUIRE(decodedTx.gasLimit == tx.gasLimit);
    REQUIRE(decodedTx.to == tx.to);
    REQUIRE(decodedTx.value == tx.value);
    REQUIRE(decodedTx.data == tx.data);
    REQUIRE(decodedTx.chainId == tx.chainId);
    REQUIRE(decodedTx.gasPrice == tx.gasPrice);
    REQUIRE(decodedTx.accessList == tx.accessList);
    REQUIRE(decodedTx.getBytePrefix() == tx.getBytePrefix());
}

TEST_CASE("Type2Tx RLP encoding with prefix", "[rlp][eth-transaction][unit][correctness]") {
    Type2Tx tx = type2TxSample;

    auto encoded = tx.rlpEncode(std::nullopt);
    REQUIRE(!encoded.empty());

    // Type2 transactions should have 0x02 prefix
    REQUIRE(encoded[0] == 0x02);

    // remove the prefix for RLPItem parsing
    encoded.erase(encoded.begin());

    // decode
    RLPItem item(encoded);
    REQUIRE(item.isList());
    Type2Tx decodedTx(item);

    // encoded tx must be equal to original
    REQUIRE(decodedTx.nonce == tx.nonce);
    REQUIRE(decodedTx.gasLimit == tx.gasLimit);
    REQUIRE(decodedTx.to == tx.to);
    REQUIRE(decodedTx.value == tx.value);
    REQUIRE(decodedTx.data == tx.data);
    REQUIRE(decodedTx.chainId == tx.chainId);
    REQUIRE(decodedTx.maxPriorityFeePerGas == tx.maxPriorityFeePerGas);
    REQUIRE(decodedTx.maxFeePerGas == tx.maxFeePerGas);
    REQUIRE(decodedTx.accessList == tx.accessList);
    REQUIRE(decodedTx.getBytePrefix() == tx.getBytePrefix());
}

TEST_CASE("Transaction RLP encoding with signature", "[rlp][eth-transaction][unit][correctness]") {
    LegacyTx tx = legacyTxSample;

    auto privateKey = getTestPrivateKey();
    Signature sig = tx.sign(privateKey);

    auto encodedWithSig = tx.rlpEncode(sig);
    auto encodedWithoutSig = tx.rlpEncode(std::nullopt);

    // Encoded transaction with signature should be longer than without
    REQUIRE(encodedWithSig.size() > encodedWithoutSig.size());
}