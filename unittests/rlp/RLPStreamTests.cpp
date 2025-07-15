//
// RLPStream Unit Tests
// Tests for RLP encoding functionality
//

#include "RLPTestUtils.h"
#include "rlp/RLPStream.h"

using namespace RLP_Tests;

// ===================== BASIC ENCODING TESTS =====================

TEST_CASE_METHOD(RLPTestFixture, "RLPStream encode empty string", "[rlp][rlp-stream][unit][correctness]") {
    RLPStream stream;
    stream << std::vector<uint8_t>{};
    
    auto encoded = stream.encode();
    
    // Empty string should encode as 0x80 in a list context
    // List with one empty item: 0xc1 (list of 1 byte) + 0x80 (empty string)
    REQUIRE(encoded.size() == 2);
    REQUIRE(encoded[0] == 0xc1);
    REQUIRE(encoded[1] == 0x80);
}

TEST_CASE_METHOD(RLPTestFixture, "RLPStream encode single byte", "[rlp][rlp-stream][unit][correctness]") {
    RLPStream stream;
    stream << std::vector<uint8_t>{0x42};
    
    auto encoded = stream.encode();
    
    // Single byte 0x42 in list: 0xc1 (list of 1 byte) + 0x42 (direct encoding)
    REQUIRE(encoded.size() == 2);
    REQUIRE(encoded[0] == 0xc1);
    REQUIRE(encoded[1] == 0x42);
}

TEST_CASE_METHOD(RLPTestFixture, "RLPStream encode low byte values", "[rlp][rlp-stream][unit][correctness]") {
    // Test bytes < 0x80 (should be encoded directly)
    for (uint8_t i = 0; i < 0x80; ++i) {
        RLPStream stream;
        stream << std::vector<uint8_t>{i};
        
        auto encoded = stream.encode();
        
        REQUIRE(encoded.size() == 2);
        REQUIRE(encoded[0] == 0xc1);  // List prefix
        REQUIRE(encoded[1] == i);     // Direct encoding
    }
}

TEST_CASE_METHOD(RLPTestFixture, "RLPStream encode short string", "[rlp][rlp-stream][unit][correctness]") {
    RLPStream stream;
    std::vector<uint8_t> data = {0x68, 0x65, 0x6c, 0x6c, 0x6f}; // "hello"
    stream << data;
    
    auto encoded = stream.encode();
    
    // Expected: 0xc6 (list of 6 bytes) + 0x85 (string of 5 bytes) + "hello"
    REQUIRE(encoded.size() == 7);
    REQUIRE(encoded[0] == 0xc6);  // List prefix
    REQUIRE(encoded[1] == 0x85);  // String prefix (0x80 + 5)
    
    // Verify the actual data
    for (size_t i = 0; i < data.size(); ++i) {
        REQUIRE(encoded[2 + i] == data[i]);
    }
}

TEST_CASE_METHOD(RLPTestFixture, "RLPStream encode multiple items", "[rlp][rlp-stream][unit][correctness]") {
    RLPStream stream;
    stream << std::vector<uint8_t>{0x01};
    stream << std::vector<uint8_t>{0x02, 0x03};
    stream << std::vector<uint8_t>{};  // empty string
    
    auto encoded = stream.encode();
    
    // Should be a list containing three items
    REQUIRE(encoded.size() >= 5);
    REQUIRE(encoded[0] >= 0xc0);  // List prefix
}

// ===================== LONG STRING ENCODING TESTS =====================

TEST_CASE_METHOD(RLPTestFixture, "RLPStream encode long string", "[rlp][rlp-stream][unit][correctness]") {
    std::vector<uint8_t> longData(100, 0x41); // 100 'A's

    RLPStream stream;
    stream << longData;

    auto encoded = stream.encode();

    REQUIRE(encoded.size() == 2 + 102);  // 2 bytes list prefix + 102 bytes inner item

    REQUIRE(encoded[0] == 0xf8);         // list prefix (long list)
    REQUIRE(encoded[1] == 0x66);         // length of list payload = 102

    REQUIRE(encoded[2] == 0xb8);         // long string prefix of inner element
    REQUIRE(encoded[3] == 0x64);         // length = 100

    // Check payload bytes match the original data
    REQUIRE(std::equal(encoded.begin() + 4, encoded.end(), longData.begin()));
}


TEST_CASE_METHOD(RLPTestFixture, "RLPStream encode boundary lengths", "[rlp][rlp-stream][unit][correctness]") {
    // Short list (payload ≤ 55)
    {
        RLPStream stream;
        stream << std::vector<uint8_t>{0x01};      // 1 byte item
        stream << std::vector<uint8_t>{0x02, 0x03}; // 2 bytes item
        auto encoded = stream.encode();

        // Sum of encoded items ≤ 55 → short list prefix: 0xc0 + payload length
        REQUIRE(encoded[0] >= 0xc0);
        REQUIRE(encoded[0] <= 0xf7);
    }

    // Long list (payload > 55)
    {
        std::vector<uint8_t> largeData(56, 0x42);
        RLPStream stream;
        stream << largeData;
        auto encoded = stream.encode();

        // Payload length > 55 → long list prefix: 0xf7 + len(len)
        REQUIRE(encoded[0] >= 0xf8);  // long list prefix range
    }
}

// ===================== NESTED STREAM TESTS =====================

TEST_CASE_METHOD(RLPTestFixture, "RLPStream encode nested streams", "[rlp][rlp-stream][unit][correctness]") {
    RLPStream innerStream;
    innerStream << std::vector<uint8_t>{'A', 'B'}; // will use 3 bytes
    innerStream << std::vector<uint8_t>{'C', 'D'}; // will use 3 bytes

    RLPStream outerStream;
    outerStream << std::vector<uint8_t>{'X', 'Y'}; // will use 3 bytes
    outerStream << innerStream;                    // will use 3 + 3 + 1 = 7 bytes
    outerStream << std::vector<uint8_t>{'Z'};      // will use 1 byte

    auto encoded = outerStream.encode();           // will use 3 + 7 + 1 + 1 = 12 bytes total 

    // Validate full structure
    REQUIRE(encoded.size() == 12);
    REQUIRE(encoded[0] == 0xcb);

    // "XY" string
    REQUIRE(encoded[1] == 0x82);
    REQUIRE(encoded[2] == 'X');
    REQUIRE(encoded[3] == 'Y');

    // Inner list ["AB", "CD"]
    REQUIRE(encoded[4] == 0xc6);

    REQUIRE(encoded[5] == 0x82);
    REQUIRE(encoded[6] == 'A');
    REQUIRE(encoded[7] == 'B');

    REQUIRE(encoded[8] == 0x82);
    REQUIRE(encoded[9] == 'C');
    REQUIRE(encoded[10] == 'D');

    // "Z" single byte encoded without prefix
    REQUIRE(encoded[11] == 'Z');
}


// ===================== U256 ENCODING TESTS =====================

TEST_CASE_METHOD(RLPTestFixture, "RLPStream encode u256 values", "[rlp][rlp-stream][unit][correctness]") {
    RLPStream stream;

    u256 zero = 0;
    u256 small = 42;
    u256 large = u256(1) << 64;

    stream << zero << small << large;
    auto encoded = stream.encode();

    // === Validate top-level structure ===
    REQUIRE(encoded.size() >= 1);
    REQUIRE(encoded[0] >= 0xc0);  // List prefix

    size_t offset = 1;

    // === Decode and validate zero ===
    REQUIRE(encoded[offset] == 0x80); // empty string for zero
    offset += 1;

    // === Decode and validate small ===
    REQUIRE(encoded[offset] == 0x2A); // direct single-byte encoding
    offset += 1;

    // === Decode and validate large ===
    REQUIRE(encoded[offset] == 0x89); // 9-byte long string
    offset += 1;
    REQUIRE(encoded.size() >= offset + 9);
    
    // Should start with 0x01 followed by 8x 0x00
    REQUIRE(encoded[offset] == 0x01);
    for (size_t i = 1; i < 9; ++i) {
        REQUIRE(encoded[offset + i] == 0x00);
    }

    offset += 9;

    // === Final size should match ===
    REQUIRE(encoded.size() == offset);
}


// ===================== PERFORMANCE TESTS =====================

TEST_CASE_METHOD(RLPTestFixture, "RLPStream performance - many small items", "[rlp][rlp-stream][unit][performance]") {
    const size_t itemCount = 10000;
    
    auto duration = measureTime([&]() {
        RLPStream stream;
        for (size_t i = 0; i < itemCount; ++i) {
            std::vector<uint8_t> data = {static_cast<uint8_t>(i % 256)};
            stream << data;
        }
        auto encoded = stream.encode();
        
        // Verify basic properties
        REQUIRE(encoded.size() > itemCount);
        REQUIRE(encoded[0] >= 0xc0);
    });
    
    // Should complete in reasonable time
    REQUIRE(duration.count() < 100);  // Less than 100 ms
    
    std::cout << "RLPStream performance: " << itemCount 
              << " items encoded in " << duration.count() << "ms" << std::endl;
}

TEST_CASE_METHOD(RLPTestFixture, "RLPStream performance - large items", "[rlp][rlp-stream][unit][performance]") {
    const size_t itemSize = 1024 * 1024; // 1MB items
    const size_t itemCount = 10;
    
    auto duration = measureTime([&]() {
        RLPStream stream;
        for (size_t i = 0; i < itemCount; ++i) {
            auto data = createTestData(itemSize, static_cast<uint8_t>(i));
            stream << data;
        }
        auto encoded = stream.encode();
        
        // Verify basic properties
        REQUIRE(encoded.size() > itemCount * itemSize);
    });
    
    // Should complete in reasonable time
    REQUIRE(duration.count() < 200);  // Less than 200ms
    
    std::cout << "RLPStream large items: " << itemCount 
              << " x " << itemSize << " bytes in " << duration.count() << "ms" << std::endl;
}
