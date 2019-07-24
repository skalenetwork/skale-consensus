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

    @file CommittedTransactionDB.h
    @author Stan Kladko
    @date 2019
*/


#ifndef SKALED_COMMITTEDTRANSACTIONDB_H
#define SKALED_COMMITTEDTRANSACTIONDB_H

class Transaction;

#include "LevelDB.h"

class CommittedTransactionDB : public LevelDB{

    const string getFormatVersion();



public:

    CommittedTransactionDB(string& filename, node_id nodeId);

    void writeCommittedTransaction(ptr<Transaction> _t, __uint64_t _committedTransactionCounter);

};



#endif //SKALED_COMMITTEDTRANSACTIONDB_H
