//
// Created by kladko on 17.06.20.
//

#include "thirdparty/catch.hpp"
#include "Consensust.h"
#include "crypto/CryptoManager.h"
#include "crypto/OpenSSLECDSAKey.h"
#include "json/JSONFactory.h"
#include "JsonStubClient.h"
#include <jsonrpccpp/client/connectors/httpclient.h>


CATCH_TEST_CASE_METHOD( StartFromScratch, "Test sgx server connection", "[sgx][end-to-end][correctness]" ) {
    auto engine = make_shared< ConsensusEngine >( 0, 100000000 );
    engine->logInit();
    jsonrpc::HttpClient client( "http://localhost:1029" );
    auto c = make_shared< StubClient >( client, jsonrpc::JSONRPC_CLIENT_V2 );

    for ( int i = 1; i <= 4; i++ ) {
        auto res = CryptoManager::generateSGXECDSAKey( c );
        CATCH_REQUIRE_FALSE( res.first.empty() );
        CATCH_REQUIRE_FALSE( res.second.empty() );

        auto msg = make_shared< vector< uint8_t > >();
        msg->push_back( static_cast< uint8_t >( i ) );
        auto hash = BLAKE3Hash::calculateHash( msg );
        auto result = c->ecdsaSignMessageHash( 16, res.first, hash.toHex() );
        JSONFactory::checkSGXStatus( result );
        auto r = JSONFactory::getString( result, "signature_r" );
        auto s = JSONFactory::getString( result, "signature_s" );
        auto v = JSONFactory::getString( result, "signature_v" );
        CATCH_REQUIRE( r.size() > 2 );
        CATCH_REQUIRE( s.size() > 2 );
        auto signature = v + ":" + r.substr( 2 ) + ":" + s.substr( 2 );

        auto key = OpenSSLECDSAKey::importSGXPubKey( res.second );
        CATCH_REQUIRE( key );
        CATCH_REQUIRE_NOTHROW(
            key->verifySGXSig( signature, reinterpret_cast< const char* >( hash.data() ) ) );
    }
}


CATCH_TEST_CASE( "Parse sgx keys", "[sgx-parse][end-to-end][correctness]" ) {
    auto serverURL = string( "http://localhost:1029" );
    auto eng = make_shared< ConsensusEngine >( 0, 100000000 );
    eng->setTestKeys( serverURL, "run_sgx_test/sgx_data/4node.json", 4, 1 );
    eng = make_shared< ConsensusEngine >( 0, 100000000 );
    eng->setTestKeys( serverURL, "run_sgx_test/sgx_data/16node.json", 16, 5 );
}