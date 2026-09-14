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

    // Non-IP error string remains unmodified
    std::string noIpErr = "Operation timed out waiting for response";
    CATCH_CHECK( CryptoManager::sanitizeIpAddress( noIpErr ) == noIpErr );
}
