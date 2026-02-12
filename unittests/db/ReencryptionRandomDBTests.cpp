/*
    Copyright (C) 2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with skale-consensus.  If not, see <https://www.gnu.org/licenses/>.

    @file ReencryptionRandomDBTests.cpp
    @author SKALE Labs
    @date 2026
*/

#ifdef BITE2

#include "SkaleCommon.h"
#include "exceptions/InvalidStateException.h"

#define BOOST_PENDING_INTEGER_LOG2_HPP

#include <boost/integer/integer_log2.hpp>


#include "thirdparty/catch.hpp"

#include "db/ReencryptionRandomDB.h"
#include "DBTestUtils.hpp"


// Test fixture for ReencryptionRandomDB tests
// Initializes the ConsensusEngine/Node/Schain once for all tests in this file
class ReencryptionRandomDBFixture {
protected:
    ReencryptionRandomDBFixture() {
        // Ensure shared DB fixture is initialized once per test process.
        ( void ) DBTestUtils::getSharedFixture();
    }
};


CATCH_TEST_CASE_METHOD( ReencryptionRandomDBFixture, "ReencryptionRandomDB: Write/Read Roundtrip",
    "[db][reencryption-db][write-read][correctness][unit]" ) {
    static string dbName = "test_reencryption_roundtrip";
    auto db = DBTestUtils::createDB< ReencryptionRandomDB >( dbName );

    // Test data: write and read a random value for a specific blockId
    block_id testBlockId = 42;
    u256 testRandom( "0x123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef" );

    db->writeRandom( testBlockId, testRandom );
    u256 readRandom = db->readRandom( testBlockId );

    CATCH_REQUIRE( readRandom == testRandom );

    DBTestUtils::cleanupDB( dbName );
}


CATCH_TEST_CASE_METHOD( ReencryptionRandomDBFixture,
    "ReencryptionRandomDB: No Overwrite Behavior", "[db][reencryption-db][overwrite][correctness][unit]" ) {
    static string dbName = "test_reencryption_overwrite";
    auto db = DBTestUtils::createDB< ReencryptionRandomDB >( dbName );

    block_id testBlockId = 100;
    u256 firstRandom( "0x1111111111111111111111111111111111111111111111111111111111111111" );
    u256 secondRandom( "0x2222222222222222222222222222222222222222222222222222222222222222" );

    // Write first value
    db->writeRandom( testBlockId, firstRandom );
    u256 readFirst = db->readRandom( testBlockId );
    CATCH_REQUIRE( readFirst == firstRandom );

    // Try to write 2nd value
    db->writeRandom( testBlockId, secondRandom );

    // No overwrite - should still read the first value
    u256 readSecond = db->readRandom( testBlockId );
    CATCH_REQUIRE( readSecond == firstRandom );

    DBTestUtils::cleanupDB( dbName );
}


CATCH_TEST_CASE_METHOD( ReencryptionRandomDBFixture, "ReencryptionRandomDB: Independent Values",
    "[db][reencryption-db][independent][correctness][unit]" ) {
    static string dbName = "test_reencryption_independent";
    auto db = DBTestUtils::createDB< ReencryptionRandomDB >( dbName );

    // Write multiple independent values
    block_id blockId1 = 10;
    block_id blockId2 = 20;
    block_id blockId3 = 30;

    u256 random1( "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" );
    u256 random2( "0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb" );
    u256 random3( "0xcccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc" );

    db->writeRandom( blockId1, random1 );
    db->writeRandom( blockId2, random2 );
    db->writeRandom( blockId3, random3 );

    // Read back and verify each value is independent
    u256 read1 = db->readRandom( blockId1 );
    u256 read2 = db->readRandom( blockId2 );
    u256 read3 = db->readRandom( blockId3 );

    CATCH_REQUIRE( read1 == random1 );
    CATCH_REQUIRE( read2 == random2 );
    CATCH_REQUIRE( read3 == random3 );
    CATCH_REQUIRE( read1 != read2 );
    CATCH_REQUIRE( read2 != read3 );
    CATCH_REQUIRE( read1 != read3 );

    DBTestUtils::cleanupDB( dbName );
}


CATCH_TEST_CASE_METHOD( ReencryptionRandomDBFixture,
    "ReencryptionRandomDB: Persistence", "[db][reencryption-db][persistence][correctness][unit]" ) {
    static string dbName = "test_reencryption_persistence";
    block_id testBlockId = 500;
    u256 testRandom( "0xdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef" );

    {
        // Create DB, write value, and let it go out of scope
        auto db = DBTestUtils::createDB< ReencryptionRandomDB >( dbName );
        db->writeRandom( testBlockId, testRandom );
    }

    {
        // Reopen the DB and verify the value persisted
        auto db = DBTestUtils::reopenDB< ReencryptionRandomDB >( dbName );
        u256 readRandom = db->readRandom( testBlockId );
        CATCH_REQUIRE( readRandom == testRandom );
    }

    DBTestUtils::cleanupDB( dbName );
}


CATCH_TEST_CASE_METHOD( ReencryptionRandomDBFixture,
    "ReencryptionRandomDB: Missing Key", "[db][reencryption-db][missing-key][error-path][unit]" ) {
    static string dbName = "test_reencryption_missing_key";
    auto db = DBTestUtils::createDB< ReencryptionRandomDB >( dbName );

    // Attempt to read a key that doesn't exist - should throw
    block_id nonExistentBlockId = 9999;

    bool exceptionThrown = false;
    try {
        db->readRandom( nonExistentBlockId );
    } catch ( const InvalidStateException& e ) {
        exceptionThrown = true;
    } catch ( const std::exception& e ) {
        exceptionThrown = true;
    }

    CATCH_REQUIRE( exceptionThrown );

    DBTestUtils::cleanupDB( dbName );
}

#endif
