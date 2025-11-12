//
// RLPItem Unit Tests  
// Tests for RLP decoding functionality and security
//

#include "RLPTestUtils.h"
#include "rlp/RLP.h"

using namespace RLP_Tests;

// ===================== BASIC DECODING TESTS =====================

CATCH_TEST_CASE_METHOD(RLPTestFixture, "RLPItem decode empty string", "[rlp][rlp-item][unit][correctness]") {
    std::vector<uint8_t> rlpData = {0xc1, 0x80}; // List with empty string
    
    RLPItem item(rlpData);
    
    CATCH_REQUIRE(item.isList());
    CATCH_REQUIRE(item.size() == 1);
    CATCH_REQUIRE_FALSE(item[0].isList());
    CATCH_REQUIRE(item[0].asBytes().empty());
}

CATCH_TEST_CASE_METHOD(RLPTestFixture, "RLPItem decode single byte", "[rlp][rlp-item][unit][correctness]") {
    std::vector<uint8_t> rlpData = {0xc1, 0x42}; // List with single byte
    
    RLPItem item(rlpData);
    
    CATCH_REQUIRE(item.isList());
    CATCH_REQUIRE(item.size() == 1);
    CATCH_REQUIRE_FALSE(item[0].isList());
    CATCH_REQUIRE(item[0].asBytes() == std::vector<uint8_t>{0x42});
}

CATCH_TEST_CASE_METHOD(RLPTestFixture, "RLPItem decode all single byte values", "[rlp][rlp-item][unit][correctness]") {
    // Test all bytes < 0x80 (should decode directly)
    for (uint8_t i = 0; i < 0x80; ++i) {
        std::vector<uint8_t> rlpData = {0xc1, i};
        
        RLPItem item(rlpData);
        
        CATCH_REQUIRE(item.isList());
        CATCH_REQUIRE(item.size() == 1);
        CATCH_REQUIRE(item[0].asBytes() == std::vector<uint8_t>{i});
    }
}

CATCH_TEST_CASE_METHOD(RLPTestFixture, "RLPItem decode short string", "[rlp][rlp-item][unit][correctness]") {
    std::vector<uint8_t> rlpData = {0xc6, 0x85, 0x68, 0x65, 0x6c, 0x6c, 0x6f}; // List with "hello"
    
    RLPItem item(rlpData);
    
    CATCH_REQUIRE(item.isList());
    CATCH_REQUIRE(item.size() == 1);
    CATCH_REQUIRE_FALSE(item[0].isList());
    
    auto bytes = item[0].asBytes();
    std::string result(bytes.begin(), bytes.end());
    CATCH_REQUIRE(result == "hello");
}

CATCH_TEST_CASE_METHOD(RLPTestFixture, "RLPItem decode multiple items", "[rlp][rlp-item][unit][correctness]") {
    // Create RLP list with multiple items: [0x01, [0x02, 0x03], ""]
    std::vector<uint8_t> rlpData = {
        0xc5,             // List of 5 bytes total
        0x01,             // First item: single byte 0x01
        0x82, 0x02, 0x03, // Second item: short string [0x02, 0x03]
        0x80              // Third item: empty string
    };
    
    RLPItem item(rlpData);
    
    CATCH_REQUIRE(item.isList());
    CATCH_REQUIRE(item.size() == 3);
    
    // Check first item
    CATCH_REQUIRE_FALSE(item[0].isList());
    CATCH_REQUIRE(item[0].asBytes() == std::vector<uint8_t>{0x01});
    
    // Check second item
    CATCH_REQUIRE_FALSE(item[1].isList());
    CATCH_REQUIRE(item[1].asBytes() == std::vector<uint8_t>{0x02, 0x03});
    
    // Check third item
    CATCH_REQUIRE_FALSE(item[2].isList());
    CATCH_REQUIRE(item[2].asBytes().empty());
}

// ===================== LONG STRING DECODING TESTS =====================

CATCH_TEST_CASE_METHOD(RLPTestFixture, "RLPItem decode long string", "[rlp][rlp-item][unit][correctness]") {
    std::vector<uint8_t> longData(100, 0x41); // 100 'A's

    std::vector<uint8_t> rlpData;
    rlpData.push_back(0xb8);     // 0xb7 + 1 (1-byte length field)
    rlpData.push_back(100);      // 0x64 = 100 in decimal
    rlpData.insert(rlpData.end(), longData.begin(), longData.end());

    
    RLPItem item(rlpData);
    
    CATCH_REQUIRE(!item.isList());
    CATCH_REQUIRE(item.asBytes() == longData);
}

CATCH_TEST_CASE_METHOD(RLPTestFixture, "RLPItem decode boundary lengths", "[rlp][rlp-item][unit][correctness]") {
    // Test 55 bytes (last short string)
    {
        std::vector<uint8_t> data(54, 0x42);
        std::vector<uint8_t> rlpData;

        // Short string header
        rlpData.push_back(0x80 + 54);

        // Data bytes
        rlpData.insert(rlpData.end(), data.begin(), data.end());

        RLPItem item(rlpData);
        CATCH_REQUIRE(!item.isList());
        CATCH_REQUIRE(item.asBytes() == data);
    }
    
    // Test 56 bytes (first long string)
    {
        std::vector<uint8_t> data(56, 0x42);
        std::vector<uint8_t> rlpData; // List header (58 = 1 + 1 + 56)
        rlpData.push_back(0xb8);                    // Long string prefix (0xb7 + 1)
        rlpData.push_back(56);                      // Length
        rlpData.insert(rlpData.end(), data.begin(), data.end());
        
        RLPItem item(rlpData);
        CATCH_REQUIRE(!item.isList());
        CATCH_REQUIRE(item.asBytes() == data);
    }
}

// ===================== NESTED LIST TESTS =====================

CATCH_TEST_CASE_METHOD(RLPTestFixture, "RLPItem decode nested lists", "[rlp][rlp-item][unit][correctness]") {
    // RLP encoding of: [[0x01, 0x02], [0x03], []]
    std::vector<uint8_t> rlpData = {
        0xc6,                   // Outer list (6 bytes payload)
        0xc2, 0x01, 0x02,      // First inner list: [0x01, 0x02]
        0xc1, 0x03,            // Second inner list: [0x03]
        0xc0                   // Third inner list: []
    };
    
    RLPItem item(rlpData);
    
    CATCH_REQUIRE(item.isList());
    CATCH_REQUIRE(item.size() == 3);
    
    // First inner list
    CATCH_REQUIRE(item[0].isList());
    CATCH_REQUIRE(item[0].size() == 2);
    CATCH_REQUIRE(item[0][0].asBytes() == std::vector<uint8_t>{0x01});
    CATCH_REQUIRE(item[0][1].asBytes() == std::vector<uint8_t>{0x02});
    
    // Second inner list
    CATCH_REQUIRE(item[1].isList());
    CATCH_REQUIRE(item[1].size() == 1);
    CATCH_REQUIRE(item[1][0].asBytes() == std::vector<uint8_t>{0x03});
    
    // Third inner list (empty)
    CATCH_REQUIRE(item[2].isList());
    CATCH_REQUIRE(item[2].size() == 0);
}


// ===================== SECURITY TESTS =====================

CATCH_TEST_CASE_METHOD(RLPTestFixture, "RLPItem security - reject oversized data", "[rlp][rlp-item][unit][security]") {
    // Create data larger than MAX_RLP_DATA_SIZE (64MB)
    auto oversizedData = SecurityTestData::createLimitTestData(100 * 1024 * 1024); // 100MB
    
    // Should throw exception due to size limit
    CATCH_REQUIRE_THROWS_AS(RLPItem(oversizedData), InvalidStateException);
}

CATCH_TEST_CASE_METHOD(RLPTestFixture, "RLPItem security - reject deep nesting", "[rlp][rlp-item][unit][security]") {
    auto deeplyNested = SecurityTestData::createDeeplyNestedRLP(2000);
    
    // Should throw exception due to nesting depth limit
    CATCH_REQUIRE_THROWS_AS(RLPItem(deeplyNested), InvalidStateException);
}

CATCH_TEST_CASE_METHOD(RLPTestFixture, "RLPItem security - handle malformed data", "[rlp][rlp-item][unit][security]") {
    // Test truncated data
    {
        auto truncated = SecurityTestData::createTruncatedRLP();
        CATCH_REQUIRE_THROWS_AS(RLPItem(truncated), InvalidStateException);
    }
    
    // Test oversized length encoding
    {
        auto oversized = SecurityTestData::createOversizedLengthRLP();
        CATCH_REQUIRE_THROWS_AS(RLPItem(oversized), InvalidStateException);
    }
    
    // Test invalid list structure
    {
        std::vector<uint8_t> invalidList = {0xc5, 0x01, 0x02}; // Claims 5 bytes but only has 2
        CATCH_REQUIRE_THROWS_AS(RLPItem(invalidList), InvalidStateException);
    }
}

CATCH_TEST_CASE_METHOD(RLPTestFixture, "RLPItem security - validate nesting depth tracking", "[rlp][rlp-item][unit][security]") {
    // Test that nesting depth is correctly tracked
    const size_t maxDepth = MAX_RLP_NESTING_DEPTH; // Just under limit
    
    auto validNested = SecurityTestData::createDeeplyNestedRLP(maxDepth);
    
    // Should parse successfully
    CATCH_REQUIRE_NOTHROW(RLPItem(validNested));
    
    // Test just over the limit
    auto invalidNested = SecurityTestData::createDeeplyNestedRLP(maxDepth + 1);
    CATCH_REQUIRE_THROWS_AS(RLPItem(invalidNested), InvalidStateException);
}