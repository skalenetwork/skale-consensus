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

    @file RandomDBTests.cpp
    @author SKALE Labs
    @date 2026
*/

#ifdef BITE2

#include "SkaleCommon.h"
#include "exceptions/InvalidStateException.h"

#define BOOST_PENDING_INTEGER_LOG2_HPP

#include <boost/integer/integer_log2.hpp>


#include "thirdparty/catch.hpp"

#include "db/RandomDB.h"
#include "DBTestUtils.hpp"


// Test fixture for RandomDB tests
// Initializes the ConsensusEngine/Node/Schain once for all tests in this file
class RandomDBFixture {
protected:
    RandomDBFixture() {
        // Ensure shared DB fixture is initialized once per test process.
        ( void ) DBTestUtils::getSharedFixture();
    }
};


CATCH_TEST_CASE_METHOD( RandomDBFixture, "RandomDB: Write/Read Roundtrip",
    "[db][random-db][correctness][unit]" ) {
    static string dbName = "test_random_roundtrip";
    auto db = DBTestUtils::createDB< RandomDB >( dbName );

    // Test data: write and read a random value for a specific blockId and domain
    block_id testBlockId = 42;
    string_view testDomain = "reencryption";
    u256 testRandom = u256( "0x123456789abcdef0" );

    db->writeDomainRandom( testDomain, testBlockId, testRandom );
    u256 readRandom = db->readDomainRandom( testDomain, testBlockId );

    CATCH_REQUIRE( readRandom == testRandom );

    DBTestUtils::cleanupDB( dbName );
}

CATCH_TEST_CASE_METHOD( RandomDBFixture, "RandomDB: U256 Write/Read Roundtrip",
    "[db][random-db][correctness][unit]" ) {
    static string dbName = "test_random_roundtrip_u256";
    auto db = DBTestUtils::createDB< RandomDB >( dbName );

    block_id testBlockId = 43;
    string_view testDomain = "reencryption";
    u256 testRandom = ( u256( 1 ) << 200 ) + 123456789;

    db->writeDomainRandom( testDomain, testBlockId, testRandom );
    u256 readRandom = db->readDomainRandom( testDomain, testBlockId );

    CATCH_REQUIRE( readRandom == testRandom );

    DBTestUtils::cleanupDB( dbName );
}

CATCH_TEST_CASE_METHOD( RandomDBFixture, "RandomDB: Domain U256 Ranges",
    "[db][random-db][u256][range][correctness][unit]" ) {
    static string dbName = "test_random_u256_ranges";
    auto db = DBTestUtils::createDB< RandomDB >( dbName );

    string_view testDomain = "reencryption";
    block_id smallU256BlockId = 44;
    block_id u256BlockId = 45;

    u256 smallRandom = u256( "0x123456789abcdef0" );
    u256 bigRandom = ( u256( 1 ) << 180 ) + 987654321;

    db->writeDomainRandom( testDomain, smallU256BlockId, smallRandom );
    CATCH_REQUIRE( db->readDomainRandom( testDomain, smallU256BlockId ) == smallRandom );

    db->writeDomainRandom( testDomain, u256BlockId, bigRandom );
    CATCH_REQUIRE( db->readDomainRandom( testDomain, u256BlockId ) == bigRandom );

    DBTestUtils::cleanupDB( dbName );
}


CATCH_TEST_CASE_METHOD( RandomDBFixture,
    "RandomDB: No Overwrite Behavior", "[db][random-db][correctness][unit]" ) {
    static string dbName = "test_random_overwrite";
    auto db = DBTestUtils::createDB< RandomDB >( dbName );

    block_id testBlockId = 100;
    string_view testDomain = "reencryption";
    u256 firstRandom = ( u256( 1 ) << 130 ) + 111;
    u256 secondRandom = ( u256( 1 ) << 131 ) + 222;

    // Write first value
    db->writeDomainRandom( testDomain, testBlockId, firstRandom );
    u256 readFirst = db->readDomainRandom( testDomain, testBlockId );
    CATCH_REQUIRE( readFirst == firstRandom );

    // Try to write 2nd value
    db->writeDomainRandom( testDomain, testBlockId, secondRandom );

    // No overwrite - should still read the first value
    u256 readSecond = db->readDomainRandom( testDomain, testBlockId );
    CATCH_REQUIRE( readSecond == firstRandom );

    DBTestUtils::cleanupDB( dbName );
}


CATCH_TEST_CASE_METHOD( RandomDBFixture, "RandomDB: Independent Values",
    "[db][random-db][correctness][unit]" ) {
    static string dbName = "test_random_independent";
    auto db = DBTestUtils::createDB< RandomDB >( dbName );

    // Write multiple independent values
    block_id blockId1 = 10;
    block_id blockId2 = 20;
    block_id blockId3 = 30;
    string_view testDomain = "reencryption";

    u256 random1 = ( u256( 1 ) << 160 ) + 1;
    u256 random2 = ( u256( 1 ) << 161 ) + 2;
    u256 random3 = ( u256( 1 ) << 162 ) + 3;

    db->writeDomainRandom( testDomain, blockId1, random1 );
    db->writeDomainRandom( testDomain, blockId2, random2 );
    db->writeDomainRandom( testDomain, blockId3, random3 );

    // Read back and verify each value is independent
    u256 read1 = db->readDomainRandom( testDomain, blockId1 );
    u256 read2 = db->readDomainRandom( testDomain, blockId2 );
    u256 read3 = db->readDomainRandom( testDomain, blockId3 );

    CATCH_REQUIRE( read1 == random1 );
    CATCH_REQUIRE( read2 == random2 );
    CATCH_REQUIRE( read3 == random3 );
    CATCH_REQUIRE( read1 != read2 );
    CATCH_REQUIRE( read2 != read3 );
    CATCH_REQUIRE( read1 != read3 );

    DBTestUtils::cleanupDB( dbName );
}


CATCH_TEST_CASE_METHOD( RandomDBFixture,
    "RandomDB: Persistence", "[db][random-db][correctness][unit]" ) {
    static string dbName = "test_random_persistence";
    block_id testBlockId = 500;
    string_view testDomain = "reencryption";
    u256 testRandom = ( u256( 1 ) << 200 ) + 55555;

    {
        // Create DB, write value, and let it go out of scope
        auto db = DBTestUtils::createDB< RandomDB >( dbName );
        db->writeDomainRandom( testDomain, testBlockId, testRandom );
    }

    {
        // Reopen the DB and verify the value persisted
        auto db = DBTestUtils::reopenDB< RandomDB >( dbName );
        u256 readRandom = db->readDomainRandom( testDomain, testBlockId );
        CATCH_REQUIRE( readRandom == testRandom );
    }

    DBTestUtils::cleanupDB( dbName );
}


CATCH_TEST_CASE_METHOD( RandomDBFixture,
    "RandomDB: Missing Key", "[db][random-db][correctness][unit]" ) {
    static string dbName = "test_random_missing_key";
    auto db = DBTestUtils::createDB< RandomDB >( dbName );

    // Attempt to read a key that doesn't exist - should throw
    block_id nonExistentBlockId = 9999;
    string_view testDomain = "reencryption";

    bool exceptionThrown = false;
    try {
        db->readDomainRandom( testDomain, nonExistentBlockId );
    } catch ( const InvalidStateException& e ) {
        exceptionThrown = true;
    } catch ( const std::exception& e ) {
        exceptionThrown = true;
    }

    CATCH_REQUIRE( exceptionThrown );

    DBTestUtils::cleanupDB( dbName );
}

#endif
