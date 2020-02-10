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

    @file Transaction.h
    @author Stan Kladko
    @date 2018
*/

#pragma  once


#define BOOST_PENDING_INTEGER_LOG2_HPP
#include <boost/integer/integer_log2.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>


#include "datastructures/DataStructure.h"

class SHAHash;




class Transaction : public DataStructure {


    static atomic<int64_t>  totalObjects;

    ptr<vector<uint8_t >> data = nullptr;

    ptr<SHAHash> hash = nullptr;

    ptr<partial_sha_hash> partialHash = nullptr;


public:

    Transaction(const ptr<vector<uint8_t>> _data, bool _includesPartialHash);


    uint64_t  getSerializedSize(bool _writePartialHash);


    ptr<vector<uint8_t>> getData() const;


    void serializeInto( ptr< vector< uint8_t > > _out, bool _writePartialHash );


    ptr<SHAHash> getHash();

    ptr<partial_sha_hash> getPartialHash();

    virtual ~Transaction();


    static ptr<Transaction > deserialize(
            const ptr< vector< uint8_t > > data, uint64_t _startIndex, uint64_t _len, bool _verifyPartialHashes );



    static int64_t getTotalObjects() {
        return totalObjects;
    };

    static ptr< Transaction > createRandomSample( uint64_t _size, boost::random::mt19937& _gen,
                                                  boost::random::uniform_int_distribution<>& _ubyte );


};



