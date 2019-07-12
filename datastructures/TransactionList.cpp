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

    @file TransactionList.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../SkaleCommon.h"
#include "../Agent.h"
#include "../Log.h"
#include "../exceptions/InvalidArgumentException.h"
#include "../exceptions/ParsingException.h"
#include "Transaction.h"
#include "TransactionList.h"


TransactionList::TransactionList(ptr<vector<ptr<Transaction>>> _transactions) {

    CHECK_ARGUMENT(_transactions != nullptr);


    totalObjects++;

    transactions = _transactions;

}


TransactionList::TransactionList( ptr<vector<uint64_t>> _transactionSizes,
    ptr<vector<uint8_t>> _serializedTransactions, uint32_t _offset, bool _checkPartialHash ) {

    if (_checkPartialHash) {
        CHECK_ARGUMENT( _serializedTransactions->size() - _offset > PARTIAL_SHA_HASH_LEN + 2 );
    } else {
        CHECK_ARGUMENT( _serializedTransactions->size() - _offset > 2 );
    }
    CHECK_ARGUMENT(_serializedTransactions->at(_offset) == '<');
    CHECK_ARGUMENT(_serializedTransactions->at(_serializedTransactions->size() - 1) == '>');



    totalObjects++;
    size_t index = _offset + 1;

    transactions = make_shared<vector<ptr<Transaction>>>();
    transactions->reserve(_transactionSizes->size());

    for (auto &&size : *_transactionSizes) {

        CHECK_ARGUMENT(size > 0);

        try {
            auto transaction =
                Transaction::deserialize( _serializedTransactions, index, size, _checkPartialHash );
            transactions->push_back(transaction);
        } catch (...) {
            throw_with_nested(ParsingException("Could not parse transaction:" + to_string(index) + ":size:" +
                             to_string(size) + ":" + to_string(_checkPartialHash), __CLASS_NAME__));
        }

        index += size;
    }


};


ptr<vector<ptr<Transaction>>> TransactionList::getItems() {
    return transactions;
}

ptr<vector<uint8_t> > TransactionList::serialize( bool _writeTxPartialHash ) {

    if (serializedTransactions)
        return serializedTransactions;


    size_t totalSize = 0;

    for (auto &&transaction : *transactions) {
        totalSize += transaction->getSerializedSize(true);
    }


    serializedTransactions = make_shared<vector<uint8_t>>();

    serializedTransactions->reserve(totalSize + 2);

    serializedTransactions->push_back('<');


    for (auto &&transaction : *transactions) {
        transaction->serializeInto( serializedTransactions, _writeTxPartialHash);
    }

    serializedTransactions->push_back('>');

    return serializedTransactions;
}

TransactionList::~TransactionList() {
    totalObjects--;
}



atomic<uint64_t>  TransactionList::totalObjects(0);

size_t TransactionList::size() {
    return transactions->size();
}


ptr<ConsensusExtFace::transactions_vector> TransactionList::createTransactionVector() {

    auto tv = make_shared< ConsensusExtFace::transactions_vector >();

    for ( auto&& t : *getItems() ) {
        tv->push_back( *( t->getData() ) );
    }
    return tv;
}
ptr< TransactionList > TransactionList::deserialize( ptr< vector< uint64_t > > _transactionSizes,
    ptr< vector< uint8_t > > _serializedTransactions, uint32_t _offset, bool _writePartialHash ) {


    for (auto &&size : *_transactionSizes) {
        CHECK_ARGUMENT(size > 0);
    }

    return ptr< TransactionList >(
        new TransactionList( _transactionSizes, _serializedTransactions, _offset, _writePartialHash ) );
}
ptr< vector< uint64_t > > TransactionList::createTransactionSizesVector(bool _writePartialHash) {
    auto ret = make_shared<vector<uint64_t>>();

    for (auto&& t : *transactions) {
        auto x= (t->getSerializedSize(_writePartialHash));
        CHECK_STATE(x > 0 );
        ret->push_back(x);
    }

    for (auto &&size : *ret) {
        CHECK_STATE( size > 0 );
    }

    return ret;

}
