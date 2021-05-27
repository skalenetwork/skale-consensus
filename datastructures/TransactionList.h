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

    @file TransactionList.h
    @author Stan Kladko
    @date 2018
*/

#pragma once

#define BOOST_PENDING_INTEGER_LOG2_HPP
#include <boost/integer/integer_log2.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>


#include "DataStructure.h"


#include "ListOfHashes.h"
#include "node/ConsensusEngine.h"


class Transaction;
class ConsensusExtFace;

class TransactionList : public ListOfHashes {

    ptr< vector< ptr< Transaction > > > transactions = nullptr; // tsafe


    ptr< vector< uint8_t > > serializedTransactions = nullptr; // tsafe
    recursive_mutex serializedTransactionsLock;

    TransactionList( const ptr< vector< uint64_t > >& _transactionSizes,
        const ptr< vector< uint8_t > >& _serializedTransactions, uint32_t _offset,
        bool _checkPartialHash );

public:

    static atomic< int64_t > totalObjects;

    explicit TransactionList( const ptr< vector< ptr< Transaction > > >& _transactions ) ;

    ptr< vector< ptr< Transaction > > > getItems();

    ptr< vector< uint8_t > > serialize( bool _writeTxPartialHash );

    size_t size();

    ~TransactionList() override;

    ptr< ConsensusExtFace::transactions_vector > createTransactionVector();

    ptr< vector< uint64_t > > createTransactionSizesVector( bool _writePartialHash );

    BLAKE3Hash getHash( uint64_t _index ) override;

    uint64_t hashCount() override;

    static int64_t getTotalObjects() { return totalObjects; }

    static ptr< TransactionList > deserialize( const ptr< vector< uint64_t > >& _transactionSizes,
        const ptr< vector< uint8_t > >& _serializedTransactions, uint32_t _offset,
        bool _writePartialHash );

    static ptr< TransactionList > createRandomSample( uint64_t _size, boost::random::mt19937& _gen,
        boost::random::uniform_int_distribution<>& _ubyte );


};
