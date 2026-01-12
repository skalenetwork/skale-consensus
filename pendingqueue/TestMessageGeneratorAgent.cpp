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

#include <array>
#include <random>
#include "thirdparty/json.hpp"
#include "Log.h"
#include "PendingTransactionsAgent.h"
#include "SkaleCommon.h"
#include "chains/Schain.h"
#include "chains/SchainTest.h"
#include "datastructures/Transaction.h"
#ifndef FAIR
#include "oracle/OracleClient.h"
#include "oracle/OracleRequestSpec.h"
#endif
#ifdef BITE
#include "rlp/EthTransactionEncoder.h"
#endif
#include "utils/Time.h"
#include "pendingqueue/TestMessageGeneratorAgent.h"


TestMessageGeneratorAgent::TestMessageGeneratorAgent( Schain& _sChain_ )
    : Agent( _sChain_, false ) {
    CHECK_STATE( _sChain_.getNodeCount() > 0 );

    // Initialize a random number generator
    std::mt19937 rng(std::random_device{}());

    // Define the distribution to generate numbers in the byte range (0 to 255)
    std::uniform_int_distribution<std::mt19937::result_type> dist(0, 255);

    // Fill the array with random bytes
    for(auto& byte : randomBytes) {
        byte = static_cast<uint8_t>(dist(rng));
    }

}


ConsensusExtFace::Transactions TestMessageGeneratorAgent::pendingTransactions(
    size_t _limit ) {
    // test oracle for the first block

    ConsensusExtFace::Transactions result;

    auto test = sChain->getBlockProposerTest();

    CHECK_STATE( !test.empty() );

    if ( test == SchainTest::NONE )
        return result;

    for ( uint64_t i = 0; i < _limit; i++ ) {
        vector< uint8_t > transaction( TEST_MESSAGE_SIZE );
        std::copy_n(randomBytes.begin() + position, TEST_MESSAGE_SIZE, transaction.begin());
        result.pushBackRegular( transaction );
        counter++;
        position = (position + randomBytes.at(position)) % (RANDOM_TEST_ARRAY_LEN - TEST_MESSAGE_SIZE - 1);
    }

    static atomic< uint64_t > iterations = 0;
    // send oracle test once from schain index 1


#ifndef FAIR
    if ( getSchain()->getNode()->isTestNet() && getSchain()->getSchainIndex() == 1 ) {
        if ( iterations.fetch_add( 1 ) == 2 ) {
            CONS_LOG( info, "Sending Oracle test eth_call " );
            sendTestRequestEthCall();
            CONS_LOG( info, "Sent Oracle eth_call request" );
        }
    }

#endif
    return result;
};


#ifndef FAIR
void TestMessageGeneratorAgent::sendTestRequestGet() {
    string uri = "https://worldtimeapi.org/api/timezone/Europe/Kiev";
    vector< string > jsps{ "/unixtime", "/day_of_year", "/xxx" };
    vector< uint64_t > trims{ 1, 1, 1 };
    string post = "";
    string encoding = "json";

    auto cid = ( uint64_t ) getSchain()->getSchainID();
    auto time = Time::getCurrentTimeMs();
    auto os = OracleRequestSpec::makeWebSpec( cid, uri, jsps, trims, post, encoding, time );

    getSchain()->getOracleClient()->sendTestRequestAndWaitForResult( os );
}


void TestMessageGeneratorAgent::sendTestRequestPost() {
    try {
        string _receipt;
        string uri = "https://reqres.in/api/users";
        vector< string > jsps = { "/id" };
        string post = "haha";
        string encoding = "json";
        auto cid = ( uint64_t ) getSchain()->getSchainID();
        auto time = Time::getCurrentTimeMs();
        auto os = OracleRequestSpec::makeWebSpec( cid, uri, jsps, {}, post, encoding, time );

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

        auto _cid = ( uint64_t ) getSchain()->getSchainID();

        auto time = Time::getCurrentTimeMs();

        auto os = OracleRequestSpec::makeEthCallSpec(
            _cid, uri, from, to, data, gas, block, encoding, time );

        getSchain()->getOracleClient()->sendTestRequestAndWaitForResult( os );

    } catch ( exception& e ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}

#endif

#ifdef BITE

ConsensusExtFace::Transactions TestMessageGeneratorAgent::pendingTransactionsBITE(
    size_t _limit ) {
    static size_t txIdxInPrecomputedBatchRegular = 0;
    static size_t txIdxInPrecomputedBatchCAT = 0;
    // contains 3/4 BITE encrypted & 1/4 unencrypted regular transactions
    static ConsensusExtFace::Transactions onlyRegularTxs;
    // contains only BITE2 CAT transactions (with encrypted args)
    static ConsensusExtFace::Transactions onlyCATs;
    static std::once_flag initFlag;
    static size_t numTotalRegularTxs = 1000;
    static size_t numTotalCATTxs = 200;
    static double CATsProportion = 0;

    // build test transactions only once at start (includes encrypting them)
    std::call_once(initFlag, [&] () {
#ifdef BITE2
        CATsProportion = (double) numTotalCATTxs / (numTotalRegularTxs + numTotalCATTxs);
        ConsensusExtFace::Transactions tmpOnlyCATs;
#endif

        ConsensusExtFace::Transactions tmpOnlyRegularTxs;

        // setup only regular txs
        for ( uint64_t i = 0; i < numTotalRegularTxs; i++ ) {
            auto tx = EthTransactionEncoder::generateSampleTx();
            // 3/4 chance of being bite encoded
            // make one quarter unencrypted
            if ( i % 4 != 0 ) {
                EthTransactionEncoder::encryptRegularTransaction( tx, sChain->getBiteManager() );
            }
            auto signedTx = EthTransactionEncoder::signAndEncodeTx( tx );
            tmpOnlyRegularTxs.emplaceBackRegular( std::move(*signedTx) );
        }
        onlyRegularTxs = std::move(tmpOnlyRegularTxs);

#ifdef BITE2
        // setup cat txs
        for ( uint64_t i = 0; i < numTotalCATTxs; i++ ) {
            auto tx = EthTransactionEncoder::generateSampleTx();
            
            // generate random CAT args & encrypt them
            EthTransactionEncoder::encryptCATTransaction( tx, sChain->getBiteManager() );
            
            auto signedTx = EthTransactionEncoder::signAndEncodeTx( tx );
            tmpOnlyCATs.emplaceBackCAT( std::move(*signedTx) );
        }
        onlyCATs = std::move(tmpOnlyCATs);
#endif
    });

    ConsensusExtFace::Transactions selectedTxs;

    auto test = sChain->getBlockProposerTest();

    CHECK_STATE( !test.empty() );

    if ( test == SchainTest::NONE )
        return selectedTxs;

    // compute how many CATs / non-CATs to include
    const size_t numCATs = (CATsProportion * _limit);
    const size_t numRegularTxs = _limit - numCATs;
    size_t catIdx = txIdxInPrecomputedBatchCAT;

#ifdef BITE2
    // place all CATs at the start
    for ( size_t i = 0; i < numCATs; i++ ) {
        catIdx = (catIdx + 1) % numTotalCATTxs;
        selectedTxs.emplaceBackCAT( onlyCATs.at(catIdx) );
    }
#endif
    ( void ) numTotalCATTxs;
    txIdxInPrecomputedBatchCAT = catIdx;

    size_t regularIdx = txIdxInPrecomputedBatchRegular;
    for ( uint64_t i = 0; i < numRegularTxs; i++ ) {
        regularIdx = (regularIdx + 1) % numTotalRegularTxs;
        selectedTxs.emplaceBackRegular( onlyRegularTxs.at(regularIdx) );
    }
    txIdxInPrecomputedBatchRegular = regularIdx;
    
    return selectedTxs;
};
#endif
