/*
    Copyright (C) 2026 SKALE Labs

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

    @file OracleParsingTests.cpp
    @date 2026
*/

// Oracle sources are not compiled in FAIR builds
#ifndef FAIR

#include "SkaleCommon.h"

#include "thirdparty/catch.hpp"
#include "thirdparty/json.hpp"

#include "chains/Schain.h"
#include "exceptions/OracleException.h"
#include "headers/BasicHeader.h"
#include "messages/NetworkMessage.h"
#include "node/ConsensusInterface.h"
#include "oracle/OracleRequestBroadcastMessage.h"
#include "oracle/OracleRequestSpec.h"
#include "oracle/OracleResult.h"

#include "tests/unit/db/DBTestUtils.hpp"

namespace {

const string TEST_URI = "http://worldtimeapi.org/api/timezone/Europe/Kiev";
const uint64_t MAX_POW_ATTEMPTS = 10000000;

/**
 * Builds a spec from the given fields (no surrounding braces, no trailing comma) and appends
 * a "pow" value that passes OracleRequestSpec::verifyPow.
 */
string addPow( const string& _fields ) {
    for ( uint64_t pow = 0; pow < MAX_POW_ATTEMPTS; pow++ ) {
        string spec = "{" + _fields + ",\"pow\":" + to_string( pow ) + "}";
        if ( OracleRequestSpec::verifyPow( spec ) ) {
            return spec;
        }
    }
    CATCH_FAIL( "Could not compute pow for test spec" );
    return "";
}

/**
 * Web spec with one jsp. _trims is inserted as the raw JSON value of "trims";
 * empty means the field is left out.
 */
string makeWebSpec( uint64_t _chainId, const string& _trims ) {
    string fields = "\"cid\":" + to_string( _chainId ) + ",\"uri\":\"" + TEST_URI +
                    "\",\"jsps\":[\"/unixtime\"],";
    if ( !_trims.empty() ) {
        fields += "\"trims\":" + _trims + ",";
    }
    fields += "\"encoding\":\"json\",\"time\":1700000000000";
    return addPow( fields );
}

/**
 * Parses the spec and returns the OracleException error code, or -1 if parsing succeeded.
 * Any other exception type propagates and fails the test.
 */
int64_t parseError( const string& _spec ) {
    try {
        OracleRequestSpec::parseSpec( _spec );
    } catch ( OracleException& e ) {
        return e.getError();
    }
    return -1;
}

string makeRequestMessage( Schain* _sChain, const string& _spec ) {
    nlohmann::json j;
    j["type"] = BasicHeader::ORACLE_REQUEST_BROADCAST;
    j["si"] = ( uint64_t ) _sChain->getSchainID();
    j["bi"] = ( uint64_t ) 1;
    j["ei"] = ( uint64_t ) 0;
    j["bpi"] = ( uint64_t ) 0;
    j["mi"] = ( uint64_t ) 1;
    j["sni"] = ( uint64_t ) 1;
    j["ssi"] = ( uint64_t ) 1;
    j["r"] = ( uint64_t ) 0;
    j["t"] = ( uint64_t ) 1700000000000;
    j["v"] = ( uint64_t ) 0;
    j["sig"] = "00";
    j["pk"] = "00";
    j["pks"] = "00";
    j["spec"] = _spec;
    return j.dump();
}

}  // namespace


CATCH_TEST_CASE( "Oracle request spec accepts well-formed web specs", "[unit][oracle]" ) {
    CATCH_SECTION( "With trims" ) {
        auto spec = OracleRequestSpec::parseSpec( makeWebSpec( 1, "[3]" ) );
        CATCH_REQUIRE( spec->getChainId() == 1 );
        CATCH_REQUIRE( spec->getJsps() == vector< string >{ "/unixtime" } );
        CATCH_REQUIRE( spec->getTrims() == vector< uint64_t >{ 3 } );
        CATCH_REQUIRE( !spec->getReceipt().empty() );
    }

    CATCH_SECTION( "Without trims" ) {
        auto spec = OracleRequestSpec::parseSpec( makeWebSpec( 1, "" ) );
        CATCH_REQUIRE( spec->getTrims() == vector< uint64_t >{ 0 } );
    }
}


CATCH_TEST_CASE( "Oracle request spec rejects trims of the wrong type", "[unit][oracle]" ) {
    const vector< string > values = { "5", "\"abc\"", "\"a\"", "{\"a\":1}", "null", "true",
        "1.5" };

    for ( const auto& trims : values ) {
        CATCH_INFO( "trims: " << trims );
        CATCH_REQUIRE( parseError( makeWebSpec( 1, trims ) ) == ORACLE_TRIMS_NOT_ARRAY );
    }
}


CATCH_TEST_CASE( "Oracle request spec rejects malformed trims arrays", "[unit][oracle]" ) {
    CATCH_SECTION( "Item is not a number" ) {
        CATCH_REQUIRE(
            parseError( makeWebSpec( 1, "[\"1\"]" ) ) == ORACLE_TRIMS_ITEM_NOT_STRING );
    }

    CATCH_SECTION( "Size differs from jsps" ) {
        CATCH_REQUIRE(
            parseError( makeWebSpec( 1, "[1,2]" ) ) == ORACLE_HSPS_TRIMS_SIZE_NOT_EQUAL );
        CATCH_REQUIRE( parseError( makeWebSpec( 1, "[]" ) ) == ORACLE_HSPS_TRIMS_SIZE_NOT_EQUAL );
    }
}


CATCH_TEST_CASE( "Oracle request spec rejects documents that are not objects", "[unit][oracle]" ) {
    const vector< string > specs = { "[1,2]", "5", "\"cid\"", "null", "true", "[]" };

    for ( const auto& spec : specs ) {
        CATCH_INFO( "spec: " << spec );
        CATCH_REQUIRE( parseError( spec ) == ORACLE_UNPARSABLE_SPEC );
    }
}


CATCH_TEST_CASE( "Oracle result rejects fields of the wrong type", "[unit][oracle]" ) {
    auto spec = OracleRequestSpec::parseSpec( makeWebSpec( 1, "" ) );

    CATCH_SECTION( "Well-formed result parses" ) {
        string result = "{\"rslts\":[\"1\",null],\"sig\":\"abc\"}";
        auto parsed = OracleResult::parseResult( result, spec );
        CATCH_REQUIRE( parsed->getSig() == "abc" );
    }

    CATCH_SECTION( "Malformed results throw" ) {
        const vector< string > results = {
            // sig is not a string
            "{\"rslts\":[\"1\"],\"sig\":5}", "{\"rslts\":[\"1\"],\"sig\":[1]}",
            "{\"rslts\":[\"1\"],\"sig\":null}",
            // rslts is not an array
            "{\"rslts\":5,\"sig\":\"abc\"}", "{\"rslts\":\"abc\",\"sig\":\"abc\"}",
            "{\"rslts\":{\"a\":1},\"sig\":\"abc\"}", "{\"rslts\":null,\"sig\":\"abc\"}",
            // document is not an object
            "[1,2]", "[\"rslts\",\"sig\"]"
        };

        for ( auto result : results ) {
            CATCH_INFO( "result: " << result );
            CATCH_REQUIRE_THROWS( OracleResult::parseResult( result, spec ) );
        }
    }
}


CATCH_TEST_CASE( "Oracle request message parses its spec on first use", "[unit][oracle]" ) {
    auto sChain = DBTestUtils::getSharedFixture().getSchain();
    auto chainId = ( uint64_t ) sChain->getSchainID();

    CATCH_SECTION( "Well-formed spec" ) {
        auto spec = makeWebSpec( chainId, "[3]" );

        auto msg = dynamic_pointer_cast< OracleRequestBroadcastMessage >(
            NetworkMessage::parseMessage( makeRequestMessage( sChain, spec ), sChain ) );

        CATCH_REQUIRE( msg );
        CATCH_REQUIRE( msg->getRequestSpec() == spec );

        auto parsed = msg->getParsedSpec();
        CATCH_REQUIRE( parsed );
        CATCH_REQUIRE( parsed->getChainId() == chainId );
        CATCH_REQUIRE( parsed->getReceipt() == OracleRequestSpec::parseSpec( spec )->getReceipt() );
        // repeated calls return the same parsed object
        CATCH_REQUIRE( msg->getParsedSpec() == parsed );
    }

    CATCH_SECTION( "Malformed spec is rejected when the spec is requested" ) {
        const vector< string > specs = { makeWebSpec( chainId, "5" ),
            makeWebSpec( chainId, "\"abc\"" ), "{\"cid\":\"x\"}", "{}" };

        for ( const auto& spec : specs ) {
            CATCH_INFO( "spec: " << spec );

            ptr< OracleRequestBroadcastMessage > msg;
            CATCH_REQUIRE_NOTHROW( msg = dynamic_pointer_cast< OracleRequestBroadcastMessage >(
                                       NetworkMessage::parseMessage(
                                           makeRequestMessage( sChain, spec ), sChain ) ) );
            CATCH_REQUIRE( msg );

            CATCH_REQUIRE_THROWS( msg->getParsedSpec() );
            // a failed parse is not cached as success
            CATCH_REQUIRE_THROWS( msg->getParsedSpec() );
        }
    }

    CATCH_SECTION( "Spec that is not enclosed in braces is rejected when the message is parsed" ) {
        CATCH_REQUIRE_THROWS(
            NetworkMessage::parseMessage( makeRequestMessage( sChain, "[1,2]" ), sChain ) );
    }
}

#endif  // FAIR
