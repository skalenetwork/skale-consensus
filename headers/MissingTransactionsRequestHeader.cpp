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

    @file MissingTransactionsRequestHeader.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"

#include "thirdparty/json.hpp"
#include "abstracttcpserver/ConnectionStatus.h"

#include "chains/Schain.h"
#include "crypto/BLAKE3Hash.h"

#include "datastructures/BlockProposal.h"
#include "chains/Schain.h"

#include "MissingTransactionsRequestHeader.h"


using namespace std;

MissingTransactionsRequestHeader::MissingTransactionsRequestHeader()
    : Header( Header::MISSING_TRANSACTIONS_REQ ) {
    missingTransactionsCount = 0;
};


MissingTransactionsRequestHeader::MissingTransactionsRequestHeader(
    const ptr< map< uint64_t, ptr< partial_sha_hash > > >& _missingMessages )
    : MissingTransactionsRequestHeader() {
    this->missingTransactionsCount = _missingMessages->size();
    complete = true;
}

void MissingTransactionsRequestHeader::addFields( nlohmann::json& _j ) {
    Header::addFields( _j );
    _j["count"] = missingTransactionsCount;
}

uint64_t MissingTransactionsRequestHeader::getMissingTransactionsCount() const {
    return missingTransactionsCount;
}

void MissingTransactionsRequestHeader::setMissingTransactionsCount(
    uint64_t _missingTransactionsCount ) {
    MissingTransactionsRequestHeader::missingTransactionsCount = _missingTransactionsCount;
}
