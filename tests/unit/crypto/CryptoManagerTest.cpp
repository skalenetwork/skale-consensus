/*
    Copyright (C) 2018-present, SKALE Labs

    This file is part of skaled.

    skaled is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skaled is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skaled.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
 * @file CryptoManagerTest.cpp
 * @date 2026
 */

#include "SkaleCommon.h"
#include "Log.h"
#include "crypto/CryptoManager.h"
#include "thirdparty/catch.hpp"

CATCH_TEST_CASE( "sanitizeIpAddress", "[crypto][unit][correctness]" ) {
    // Plain IPv4 with port
    CATCH_CHECK(
        CryptoManager::sanitizeIpAddress( "Could not connect to 192.168.1.1:8888" ) ==
        "Could not connect to [redacted]" );

    // Plain IPv4 without port
    CATCH_CHECK(
        CryptoManager::sanitizeIpAddress( "Failed to reach 90.16.6.5 after timeout" ) ==
        "Failed to reach [redacted] after timeout" );

    // URL with http scheme and port
    CATCH_CHECK(
        CryptoManager::sanitizeIpAddress(
            "libcurl error: 7 -> Could not connect to http://127.0.0.1:7765" ) ==
        "libcurl error: 7 -> Could not connect to [redacted]" );

    // Real jsonrpc / libcurl exception format
    CATCH_CHECK(
        CryptoManager::sanitizeIpAddress(
            "Exception -32003 : Client connector error: libcurl error: 7 -> Could not connect to "
            "https://127.0.0.1:0087" ) ==
        "Exception -32003 : Client connector error: libcurl error: 7 -> Could not connect to "
        "[redacted]" );

    // Multiple IPs in one string
    CATCH_CHECK(
        CryptoManager::sanitizeIpAddress(
            "Failed connection between 192.168.1.1:1234 and 100.0.50.0:3456" ) ==
        "Failed connection between [redacted] and [redacted]" );

    // Domain name with scheme and port
    CATCH_CHECK(
        CryptoManager::sanitizeIpAddress(
            "Could not connect to https://sgx-node.example.com:1026" ) ==
        "Could not connect to [redacted]" );

    // localhost with scheme and port
    CATCH_CHECK(
        CryptoManager::sanitizeIpAddress( "Could not connect to http://localhost:1029" ) ==
        "Could not connect to [redacted]" );

    // Domain name with scheme and path, no port
    CATCH_CHECK(
        CryptoManager::sanitizeIpAddress(
            "Could not connect to https://sgx-node.example.com/api" ) ==
        "Could not connect to [redacted]/api" );

    // Non-IP error string remains unmodified
    std::string noIpErr = "Operation timed out waiting for response";
    CATCH_CHECK( CryptoManager::sanitizeIpAddress( noIpErr ) == noIpErr );
}

CATCH_TEST_CASE( "RETRY_END rethrows a sanitized exception on non-retryable error",
    "[crypto][unit][correctness]" ) {
    std::string rawMsg = "Exception -32003 : Unhandled fatal error at https://192.168.1.100:1234/endpoint";

    bool caught = false;
    try {
        RETRY_BEGIN
            // Not one of the retryable patterns ("Could not connect", "libcurl error: 56/35/52",
            // "timed out"), so this hits the non-retryable else branch in RETRY_END.
            throw std::runtime_error( rawMsg );
        RETRY_END
    } catch ( const std::exception& e ) {
        caught = true;

        // The rethrown exception must no longer contain the raw IP/endpoint.
        CATCH_CHECK( std::string( e.what() ).find( "192.168.1.100" ) == std::string::npos );
        CATCH_CHECK(
            std::string( e.what() ) ==
            "Exception -32003 : Unhandled fatal error at [redacted]/endpoint" );
    }

    CATCH_CHECK( caught );
}
