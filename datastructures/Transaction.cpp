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

    @file Transaction.cpp
    @author Stan Kladko
    @date 2018
*/






#include "../SkaleCommon.h"
#include "../chains/Schain.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../crypto/SHAHash.h"

#include "Transaction.h"

ptr<SHAHash> Transaction::getHash() {

    if (hash)
        return hash;

    auto digest = make_shared<array<uint8_t , SHA3_HASH_LEN>>();


    CryptoPP::SHA3_Final< SHA3_HASH_LEN > hashObject;

    hashObject.Update(data.get()->data(), data->size());
    hashObject.Final(digest->data());



    hash = make_shared<SHAHash>(digest);

    return hash;

}


ptr<partial_sha_hash> Transaction::getPartialHash() {

    if (partialHash) {
        return partialHash;
    }

    partialHash = make_shared<partial_sha_hash>();

    getHash();

    for (size_t i = 0; i < PARTIAL_SHA_HASH_LEN; i++) {
        partialHash->at(i) = hash->at(i);
    }

    return partialHash;

}

Transaction::Transaction(const ptr<vector<uint8_t>> _data) : data(_data) {
    ASSERT(data != nullptr && data->size() > 0);
};

ptr<vector<uint8_t>> Transaction::getData() const {
    return data;
}

Transaction::~Transaction() {

}
uint64_t Transaction::getSerializedSize() {
    return data->size();
}
void Transaction::serializeInto( ptr< vector< uint8_t > > _out ) {
    ASSERT(_out != nullptr)
    _out->insert(_out->end(), data->begin(), data->end());

}
