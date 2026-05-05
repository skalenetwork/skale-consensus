//
// RLP Test Suite - Common Test Utilities and Fixtures
// Shared utilities for all RLP-related tests
//

#pragma once

#include "thirdparty/catch.hpp"
#include <vector>
#include <string>
#include <chrono>
#include <iostream>
#include <cstdio>

namespace RLP_Tests {

/**
 * @brief Common test fixture for RLP tests
 * Provides utilities used across all RLP test files
 */
class RLPTestFixture {
public:
    // Helper function to create byte vector from hex string
    std::vector<uint8_t> hexToBytes(const std::string& hex) {
        std::vector<uint8_t> bytes;
        for (size_t i = 0; i < hex.length(); i += 2) {
            std::string byteString = hex.substr(i, 2);
            uint8_t byte = static_cast<uint8_t>(strtol(byteString.c_str(), nullptr, 16));
            bytes.push_back(byte);
        }
        return bytes;
    }
    
    // Helper function to convert bytes to hex string
    std::string bytesToHex(const std::vector<uint8_t>& bytes) {
        std::string hex;
        char buf[3];
        for (uint8_t byte : bytes) {
            sprintf(buf, "%02x", byte);
            hex += buf;
        }
        return hex;
    }
    
    // Helper to create test data of specific size
    std::vector<uint8_t> createTestData(size_t size, uint8_t fillByte = 0xAA) {
        return std::vector<uint8_t>(size, fillByte);
    }
    
    // Helper to create repeating pattern data
    std::vector<uint8_t> createPatternData(size_t size, const std::vector<uint8_t>& pattern) {
        std::vector<uint8_t> data;
        data.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            data.push_back(pattern[i % pattern.size()]);
        }
        return data;
    }
    
    // Performance measurement helper
    template<typename Func>
    std::chrono::milliseconds measureTime(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    }
};

/**
 * @brief Security test data generator
 */
class SecurityTestData {
public:
    // Generate deeply nested structure for testing depth limits
    static std::vector<uint8_t> createDeeplyNestedRLP(size_t depth) {
        std::vector<uint8_t> data = {0x01}; // Innermost item

        for (size_t i = 0; i < depth; ++i) {
            std::vector<uint8_t> wrapped;

            // Each layer wraps the previous one as a single-item list.
            // Length of inner payload
            size_t len = data.size();
            if (len < 56) {
                wrapped.push_back(static_cast<uint8_t>(0xc0 + len)); // Short list prefix
            } else {
                // For completeness (though not needed for small depth)
                std::vector<uint8_t> lenBytes;
                size_t tmp = len;
                while (tmp > 0) {
                    lenBytes.insert(lenBytes.begin(), static_cast<uint8_t>(tmp & 0xFF));
                    tmp >>= 8;
                }
                wrapped.push_back(static_cast<uint8_t>(0xf7 + lenBytes.size()));
                wrapped.insert(wrapped.end(), lenBytes.begin(), lenBytes.end());
            }

            wrapped.insert(wrapped.end(), data.begin(), data.end());
            data = std::move(wrapped);
        }

        return data;
    }

    
    // Generate malformed RLP data for security testing
    static std::vector<uint8_t> createTruncatedRLP() {
        return {0x85}; // Claims 5 bytes but provides none
    }
    
    static std::vector<uint8_t> createOversizedLengthRLP() {
        return {0xbf, 0xff, 0xff, 0xff, 0xff}; // Huge length encoding
    }
    
    // Generate data just under/over security limits
    static std::vector<uint8_t> createLimitTestData(size_t targetSize) {
        std::vector<uint8_t> data;
        data.reserve(targetSize);
        
        for (size_t i = 0; i < targetSize; ++i) {
            data.push_back(static_cast<uint8_t>(i % 256));
        }
        
        return data;
    }
};

} // namespace RLP_Tests
