/*
    Copyright (C) 2018-2019 SKALE Labs

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

    @file TestMessageGeneratorAgent.cpp
    @author Stan Kladko
    @date 2018
*/

#include "thirdparty/json.hpp"
#include "Log.h"
#include "PendingTransactionsAgent.h"
#include "SkaleCommon.h"
#include "chains/Schain.h"
#include "chains/SchainTest.h"
#include "datastructures/Transaction.h"
#include "exceptions/FatalError.h"
#include "node/ConsensusEngine.h"
#include "oracle/OracleClient.h"
#include "oracle/OracleRequestSpec.h"
#include "utils/Time.h"
#include "pendingqueue/TestMessageGeneratorAgent.h"



TestMessageGeneratorAgent::TestMessageGeneratorAgent( Schain& _sChain_ )
    : Agent( _sChain_, false ) {
    CHECK_STATE( _sChain_.getNodeCount() > 0 );
}


ConsensusExtFace::transactions_vector TestMessageGeneratorAgent::pendingTransactions(
    size_t _limit ) {
    // test oracle for the first block

    uint64_t messageSize = 200;

    ConsensusExtFace::transactions_vector result;

    auto test = sChain->getBlockProposerTest();

    CHECK_STATE( !test.empty() );

    if ( test == SchainTest::NONE )
        return result;

    for ( uint64_t i = 0; i < _limit; i++ ) {
        vector< uint8_t > transaction( messageSize );

        uint64_t dummy = counter;
        auto bytes = ( uint8_t* ) &dummy;

        for ( uint64_t j = 0; j < messageSize / 8; j++ ) {
            for ( int k = 0; k < 7; k++ ) {
                transaction.at( 2 * j + k ) = bytes[k];
            }
        }

        result.push_back( transaction );

        counter++;
    }

    static atomic< uint64_t > iterations = 0;
    // send oracle test once from schain index 1

    if (getSchain()->getNode()->isTestNet() && getSchain()->getSchainIndex() == 1 ) {
        if ( iterations.fetch_add( 1 ) == 2 ) {
            LOG( info, "Sending Oracle test eth_call " );
            sendTestRequestGet();
            LOG( info, "Sent Oracle eth_call request" );
        }
    }

    return result;
};


void TestMessageGeneratorAgent::sendTestRequestGet() {
    string uri = "https://worldtimeapi.org/api/timezone/Europe/Kiev";
    vector< string > jsps{ "/unixtime", "/day_of_year", "/xxx" };
    vector< uint64_t > trims{ 1, 1, 1 };
    string post = "";
    string encoding = "json";

    auto cid = (uint64_t) getSchain()->getSchainID();
    auto time = Time::getCurrentTimeMs();
    auto os = OracleRequestSpec::makeWebSpec(cid, uri, jsps, trims, post, encoding, time);

    getSchain()->getOracleClient()->sendTestRequestAndWaitForResult( os );
}


void TestMessageGeneratorAgent::sendTestRequestPost() {
    try {


        string _receipt;
        string uri = "https://reqres.in/api/users";
        vector< string > jsps = { "/id" };
        string post = "haha";
        string encoding = "json";
        auto cid = (uint64_t) getSchain()->getSchainID();
        auto time = Time::getCurrentTimeMs();
        auto os = OracleRequestSpec::makeWebSpec(cid, uri, jsps, {}, post, encoding, time);

        getSchain()->getOracleClient()->sendTestRequestAndWaitForResult( os );

    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


void TestMessageGeneratorAgent::sendTestRequestEthCall() {
    try {

        string _receipt;
        string uri = "http://127.0.0.1:8545/";
        string from = "0x9876543210987654321098765432109876543210";
        string to = "0x5FbDB2315678afecb367f032d93F642f64180aa3";
        string data = "0x893d20e8";
        string gas = "0x100000";
        string block = "latest";
        string encoding = "json";

        auto _cid = (uint64_t) getSchain()->getSchainID();

        auto time = Time::getCurrentTimeMs();

        auto os = OracleRequestSpec::makeEthCallSpec(_cid, uri, from, to, data, gas, block,
            encoding, time);

        getSchain()->getOracleClient()->sendTestRequestAndWaitForResult( os );

    } catch ( exception& e ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}