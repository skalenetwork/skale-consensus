/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skale-consensus.  If not, see <http://www.gnu.org/licenses/>.

    @file PendingTransaction.h
    @author Stan Kladko
    @date 2018
*/

#pragma once


#include "Transaction.h"

class PendingTransaction  : public Transaction{
public:
    PendingTransaction(const ptr<vector<uint8_t>> data);



    static atomic<uint64_t>  totalObjects;

    static uint64_t getTotalObjects() {
        return totalObjects;
    };

    virtual ~PendingTransaction();

};