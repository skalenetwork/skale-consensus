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

    @file Transaction.cpp
    @author Stan Kladko
    @date 2018
*/


#define BOOST_PENDING_INTEGER_LOG2_HPP
#include <boost/integer/integer_log2.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>



#include "../thirdparty/catch.hpp"


#include "../SkaleCommon.h"
#include "../Log.h"
#include "../chains/Schain.h"
#include "../crypto/SHAHash.h"



#include "Transaction.h"



ptr< SHAHash > Transaction::getHash() {

    lock_guard<recursive_mutex> lock(m);

    if ( hash )
        return hash;

    hash = SHAHash::calculateHash(data->data(), data->size());
    return hash;

}


ptr< partial_sha_hash > Transaction::getPartialHash() {
    if ( partialHash ) {
        return partialHash;
    }

    partialHash = make_shared< partial_sha_hash >();

    getHash();

    for ( size_t i = 0; i < PARTIAL_SHA_HASH_LEN; i++ ) {
        partialHash->at( i ) = hash->at( i );
    }

    return partialHash;
}

Transaction::Transaction( const ptr< vector< uint8_t > > _trx, bool _includesPartialHash ) {


    CHECK_ARGUMENT(_trx != nullptr);


    array< uint8_t, PARTIAL_SHA_HASH_LEN > incomingHash;

    if (_includesPartialHash) {
        CHECK_ARGUMENT(_trx->size() > PARTIAL_SHA_HASH_LEN);

        std::copy( _trx->begin() + +_trx->size() - PARTIAL_SHA_HASH_LEN, _trx->end(),
                   incomingHash.begin() );


        _trx->resize( _trx->size() - PARTIAL_SHA_HASH_LEN );
    } else {
        CHECK_ARGUMENT(_trx->size() > 0);
    };



    data = _trx;




    if (_includesPartialHash) {
        auto h = getPartialHash();

        CHECK_ARGUMENT2(*h == incomingHash, "Transaction partial hash does not match");

    }

    CHECK_STATE(data != nullptr);
    CHECK_STATE(data->size() > 0);




    totalObjects++;
};


ptr< vector< uint8_t > > Transaction::getData() const {
    CHECK_STATE(data != nullptr);
    CHECK_STATE(data->size() > 0);
    return data;
}



atomic<uint64_t>  Transaction::totalObjects(0);


Transaction::~Transaction() {
    totalObjects--;

}
uint64_t Transaction::getSerializedSize(bool _writePartialHash) {

    CHECK_STATE(data->size() > 0);

    if (_writePartialHash)
        return data->size() + PARTIAL_SHA_HASH_LEN;
    return data->size();
}

void Transaction::serializeInto( ptr< vector< uint8_t > > _out, bool _writePartialHash ) {
    CHECK_ARGUMENT( _out != nullptr )
    _out->insert( _out->end(), data->begin(), data->end() );

    if (_writePartialHash) {

        auto h = getPartialHash();
        _out->insert( _out->end(), h->begin(), h->end() );
    }
}




ptr<Transaction > Transaction::deserialize(
    const ptr< vector< uint8_t > > data, uint64_t _startIndex, uint64_t _len, bool _verifyPartialHashes ) {

    CHECK_ARGUMENT(data != nullptr);

    CHECK_ARGUMENT2(_startIndex + _len <= data->size(),
                    to_string(_startIndex) + " " + to_string(_len) + " " +
                    to_string(data->size()))

    CHECK_ARGUMENT(_len > 0);

    auto transactionData = make_shared<vector<uint8_t>>(data->begin() + _startIndex,
                                                        data->begin() + _startIndex + _len);



    return ptr<Transaction>(new Transaction(transactionData, _verifyPartialHashes));

}


ptr< Transaction > Transaction::createRandomSample( uint64_t _size, boost::random::mt19937& _gen,
                                                    boost::random::uniform_int_distribution<>& _ubyte ) {
    auto sample = make_shared< vector< uint8_t > >( _size, 0 );


    for ( uint32_t j = 0; j < sample->size(); j++ ) {
        sample->at( j ) = _ubyte( _gen );
    }


    return Transaction::deserialize( sample, 0, sample->size(), false );
};
