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

    @file PartialHashesList.h
    @author Stan Kladko
    @date 2018
*/

#pragma once

#include "DataStructure.h"


class PartialHashesList : public DataStructure  {

    transaction_count transactionCount;

    ptr<vector<uint8_t>> partialHashes;

public:



    transaction_count getTransactionCount() const;

    ptr<vector<uint8_t>> getPartialHashes() const;

    virtual ~PartialHashesList();

    PartialHashesList(transaction_count _transactionCount);

    PartialHashesList(transaction_count _transactionCount, const ptr<vector<uint8_t>>& _partialHashes);

    msg_len getLen() {
        return msg_len(partialHashes->size());

    }

    ptr<partial_sha_hash> getPartialHash(uint64_t i) ;

};



