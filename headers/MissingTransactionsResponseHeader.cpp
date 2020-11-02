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

    @file MissingTransactionsResponseHeader.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"

#include "thirdparty/json.hpp"

#include "abstracttcpserver/ConnectionStatus.h"
#include "MissingTransactionsResponseHeader.h"

using namespace std;

MissingTransactionsResponseHeader::MissingTransactionsResponseHeader() : Header(MISSING_TRANSACTIONS_RSP) {

}

MissingTransactionsResponseHeader::MissingTransactionsResponseHeader(const ptr<vector<uint64_t>>& _missingTransactionSizes)
        : MissingTransactionsResponseHeader() {
    CHECK_ARGUMENT(_missingTransactionSizes);
    missingTransactionSizes = _missingTransactionSizes;
    complete = true;
}

void MissingTransactionsResponseHeader::addFields(nlohmann::basic_json<> &_j) {

    Header::addFields(_j);

    list<uint64_t> l(missingTransactionSizes->begin(), missingTransactionSizes->end());

    _j["sizes"] = l;

}



