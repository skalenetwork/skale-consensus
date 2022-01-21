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

    @file BlockDecryptResponseHeader.cpp
    @author Stan Kladko
    @date 2019
*/


#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/InvalidArgumentException.h"
#include "thirdparty/json.hpp"

#include "AbstractBlockRequestHeader.h"
#include "BlockDecryptResponseHeader.h"

BlockDecryptResponseHeader::BlockDecryptResponseHeader() : Header(Header::BLOCK_DECRYPT_RSP ) {

}


void BlockDecryptResponseHeader::addFields(nlohmann::json &_j) {


    CHECK_STATE(isComplete())
    Header::addFields(_j);

    if (getStatusSubStatus().first != CONNECTION_PROCEED)
        return;

    auto sharesArray = nlohmann::json::array();

    for (auto&& encryptedKey : decryptionShares) {
        sharesArray.push_back(encryptedKey);
    }

    _j["decryptionShares"] = sharesArray;

    setComplete();
}

void BlockDecryptResponseHeader::setDecryptionShares(const vector<string> &_decryptionShares) {
    decryptionShares = _decryptionShares;
}