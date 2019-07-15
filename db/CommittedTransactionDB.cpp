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

    @file CommittedTransactionDB.cpp
    @author Stan Kladko
    @date 2019
*/


#include "../SkaleCommon.h"
#include "../Log.h"

#include "../datastructures/Transaction.h"


#include "CommittedTransactionDB.h"




CommittedTransactionDB::CommittedTransactionDB(string& filename, node_id _nodeId ) : LevelDB( filename, _nodeId ) {}


const string CommittedTransactionDB::getFormatVersion() {
    return "1.0";
}



void CommittedTransactionDB::writeCommittedTransaction(ptr<Transaction> _t, __uint64_t _committedTransactionCounter) {

    auto key = (const char *) _t->getPartialHash()->data();
    auto keyLen = PARTIAL_SHA_HASH_LEN;
    auto value = (const char*) &_committedTransactionCounter;
    auto valueLen = sizeof(_committedTransactionCounter);
    writeByteArray(key, keyLen, value, valueLen);

    static auto key1 = string("transactions");
    auto value1 = to_string(_committedTransactionCounter);
    writeString(key1, value1);

}
