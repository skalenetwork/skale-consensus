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

    @file ImportedTransaction.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../SkaleCommon.h"
#include "../Log.h"
#include "../exceptions/InvalidArgumentException.h"
#include "ImportedTransaction.h"


ImportedTransaction::ImportedTransaction(const ptr<vector<uint8_t>> data) : Transaction(data, true) {
    totalObjects++;
}



ImportedTransaction::~ImportedTransaction() {
    totalObjects--;
}


atomic<uint64_t>  ImportedTransaction::totalObjects(0);

ptr< ImportedTransaction > ImportedTransaction::deserialize(
    const ptr< vector< uint8_t > > data, uint64_t _startIndex, uint64_t _len ) {

    CHECK_ARGUMENT(data != nullptr);

    CHECK_ARGUMENT2(_startIndex + _len <= data->size(),
                    to_string(_startIndex) + " " + to_string(_len) + " " +
                    to_string(data->size()))

    CHECK_ARGUMENT(_len > 0);

    auto transactionData = make_shared<vector<uint8_t>>(data->begin() + _startIndex,
                                                        data->begin() + _startIndex + _len);



    return ptr<ImportedTransaction>(new ImportedTransaction(transactionData));

}
