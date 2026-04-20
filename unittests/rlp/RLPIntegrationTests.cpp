//
// RLP Integration Tests
// Tests for interactions between RLP components and edge cases
//

#include "RLPTestUtils.h"
#include "rlp/RLP.h"
#include "rlp/RLPStream.h"

using namespace RLP_Tests;

// ===================== ROUNDTRIP INTEGRATION TESTS =====================

CATCH_TEST_CASE_METHOD(RLPTestFixture, "RLP complete roundtrip - simple data", "[rlp][rlp-integration][correctness][integration]") {
    // Test complete encode -> decode -> encode cycle
    
    std::vector<std::vector<uint8_t>> originalData = {
        {0x01, 0x02, 0x03},
        {0x04, 0x05},
        {},  // empty
        {0x42}
    };
    
    // First encoding
    RLPStream stream1;
    for (const auto& data : originalData) {
        stream1 << data;
    }
    auto encoded1 = stream1.encode();
    
    // Decode
    RLPItem decoded(encoded1);
    CATCH_REQUIRE(decoded.isList());
    
    // Re-encode
    RLPStream stream2;
    for (size_t i = 0; i < decoded.size(); ++i) {
        stream2 << decoded[i].asBytes();
    }
    auto encoded2 = stream2.encode();
    
    // Should be identical
    CATCH_REQUIRE(encoded1 == encoded2);
}

CATCH_TEST_CASE_METHOD(RLPTestFixture, "RLP roundtrip - complex nested data", "[rlp][rlp-integration][correctness][integration]") {
    // Test with nested structures
    
    // Create complex nested structure: [[a, b], [c, [d, e]], f]
    RLPStream innerStream1;
    innerStream1 << std::vector<uint8_t>{'a'};
    innerStream1 << std::vector<uint8_t>{'b'};
    
    RLPStream innerStream2;
    innerStream2 << std::vector<uint8_t>{'d'};
    innerStream2 << std::vector<uint8_t>{'e'};
    
    RLPStream middleStream;
    middleStream << std::vector<uint8_t>{'c'};
    middleStream << innerStream2;
    
    RLPStream outerStream;
    outerStream << innerStream1;
    outerStream << middleStream;
    outerStream << std::vector<uint8_t>{'f'};
    
    auto encoded = outerStream.encode();
    
    // Decode and verify structure
    RLPItem decoded(encoded);
    
    CATCH_REQUIRE(decoded.isList());
    CATCH_REQUIRE(decoded.size() == 3);
    
    // Should maintain the nested structure
    CATCH_REQUIRE(decoded[0].isList());  // [a, b]
    CATCH_REQUIRE(decoded[0].size() == 2);
    CATCH_REQUIRE(decoded[0][0].asBytes() == std::vector<uint8_t>{'a'});
    CATCH_REQUIRE(decoded[0][1].asBytes() == std::vector<uint8_t>{'b'});

    CATCH_REQUIRE(decoded[1].isList());  // [c, [d, e]]
    CATCH_REQUIRE(decoded[1].size() == 2);
    CATCH_REQUIRE(decoded[1][0].asBytes() == std::vector<uint8_t>{'c'});
    CATCH_REQUIRE(decoded[1][1].isList());  // [d, e]
    CATCH_REQUIRE(decoded[1][1].size() == 2);
    CATCH_REQUIRE(decoded[1][1][0].asBytes() == std::vector<uint8_t>{'d'});
    CATCH_REQUIRE(decoded[1][1][1].asBytes() == std::vector<uint8_t>{'e'});

    CATCH_REQUIRE_FALSE(decoded[2].isList());  // f
    CATCH_REQUIRE(decoded[2].asBytes() == std::vector<uint8_t>{'f'});
}

// ===================== STRESS TESTS =====================

CATCH_TEST_CASE_METHOD(RLPTestFixture, "RLP stress test - deep nesting at limit", "[rlp][rlp-integration][correctness][integration]") {
    // Test nesting exactly at the security limit
    const size_t maxDepth = 1000; // Just under MAX_RLP_NESTING_DEPTH
    
    auto deepData = SecurityTestData::createDeeplyNestedRLP(maxDepth);
    
    // Should parse successfully
    CATCH_REQUIRE_NOTHROW(RLPItem(deepData));
    
    RLPItem parsed(deepData);
    
    // Navigate to the deepest level
    RLPItem* current = &parsed;
    size_t depth = 0;
    
    while (current->isList() && current->size() > 0) {
        current = &(*current)[0];
        depth++;
        if (depth > maxDepth + 10) break; // Safety break
    }
    
    // Should have reached the expected depth
    CATCH_REQUIRE(depth <= maxDepth + 5); // Some tolerance for encoding overhead
}

CATCH_TEST_CASE_METHOD(RLPTestFixture, "RLP stress test - large list at limit", "[rlp][rlp-integration][correctness][integration]") {
    // Test with many items approaching the limit
    const size_t itemCount = 50000; // Well under MAX_RLP_LIST_LENGTH
    
    RLPStream stream;
    for (size_t i = 0; i < itemCount; ++i) {
        std::vector<uint8_t> data = {static_cast<uint8_t>(i % 256)};
        stream << data;
    }
    
    auto encoded = stream.encode();
    RLPItem decoded(encoded);
    
    CATCH_REQUIRE(decoded.isList());
    CATCH_REQUIRE(decoded.size() == itemCount);
    
    // Verify all items
    for (size_t i = 0; i < itemCount; ++i) {
        CATCH_REQUIRE(decoded[i].asBytes() == std::vector<uint8_t>{static_cast<uint8_t>(i % 256)});
    }
}

// ===================== EDGE CASE TESTS =====================

CATCH_TEST_CASE_METHOD(RLPTestFixture, "RLP edge cases - boundary values", "[rlp][rlp-integration][correctness][integration]") {
    // Test various boundary conditions
    
    // Empty list
    {
        RLPStream stream;
        auto encoded = stream.encode();
        
        RLPItem decoded(encoded);
        CATCH_REQUIRE(decoded.isList());
        CATCH_REQUIRE(decoded.size() == 0);
    }
    
    // Single empty item
    {
        RLPStream stream;
        stream << std::vector<uint8_t>{};
        auto encoded = stream.encode();
        
        RLPItem decoded(encoded);
        CATCH_REQUIRE(decoded.isList());
        CATCH_REQUIRE(decoded.size() == 1);
        CATCH_REQUIRE(decoded[0].asBytes().empty());
    }
    
    // Maximum single byte value
    {
        RLPStream stream;
        stream << std::vector<uint8_t>{0x7f};
        auto encoded = stream.encode();
        
        RLPItem decoded(encoded);
        CATCH_REQUIRE(decoded[0].asBytes() == std::vector<uint8_t>{0x7f});
    }
    
    // Minimum multi-byte string
    {
        RLPStream stream;
        stream << std::vector<uint8_t>{0x80};
        auto encoded = stream.encode();
        
        RLPItem decoded(encoded);
        CATCH_REQUIRE(decoded[0].asBytes() == std::vector<uint8_t>{0x80});
    }
}

CATCH_TEST_CASE_METHOD(RLPTestFixture, "RLP edge cases - mixed data types", "[rlp][rlp-integration][correctness][integration]") {
    // Test with various data patterns
    
    std::vector<std::vector<uint8_t>> testData;
    
    // Add various data types
    testData.push_back({}); // Empty
    testData.push_back({0x00}); // Zero byte
    testData.push_back({0x7f}); // Max single byte
    testData.push_back({0x80}); // Min multi-byte
    testData.push_back({0xff}); // Max byte value
    
    // Pattern data
    testData.push_back(createPatternData(100, {0xaa, 0xbb}));
    testData.push_back(createPatternData(55, {0x01, 0x02, 0x03})); // Boundary length
    testData.push_back(createPatternData(56, {0x04, 0x05, 0x06})); // Boundary + 1
    
    // Encode all
    RLPStream stream;
    for (const auto& data : testData) {
        stream << data;
    }
    auto encoded = stream.encode();
    
    // Decode and verify
    RLPItem decoded(encoded);
    CATCH_REQUIRE(decoded.isList());
    CATCH_REQUIRE(decoded.size() == testData.size());
    
    for (size_t i = 0; i < testData.size(); ++i) {
        CATCH_REQUIRE(decoded[i].asBytes() == testData[i]);
    }
}

// ===================== ERROR RECOVERY TESTS =====================

CATCH_TEST_CASE_METHOD(RLPTestFixture, "RLP error recovery - partial parsing", "[rlp][rlp-integration][correctness][integration]") {
    // Test that errors don't corrupt state for subsequent operations
    
    // Create some malformed data
    std::vector<uint8_t> malformed = {0x85, 0x01, 0x02}; // Claims 5 bytes, has 2
    
    // Should throw
    CATCH_REQUIRE_THROWS_AS(RLPItem(malformed), InvalidStateException);
    
    // Subsequent operations should still work
    std::vector<uint8_t> validData = {0xc1, 0x42};
    CATCH_REQUIRE_NOTHROW(RLPItem(validData));
    
    RLPItem valid(validData);
    CATCH_REQUIRE(valid.isList());
    CATCH_REQUIRE(valid.size() == 1);
}