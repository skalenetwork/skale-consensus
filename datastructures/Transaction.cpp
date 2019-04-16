//
// Created by stan on 14.03.18.
//






#include "../SkaleConfig.h"
#include "../chains/Schain.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../crypto/SHAHash.h"

#include "Transaction.h"

ptr<SHAHash> Transaction::getHash() {

    ASSERT(data && data->size() > 0);


    if (hash)
        return hash;

    auto digest = make_shared<array<uint8_t , SHA3_HASH_LEN>>();

    CryptoPP::SHA3 hashObject(SHA3_HASH_LEN);

    hashObject.Update(data.get()->data(), data->size());
    hashObject.Final(digest->data());



    hash = make_shared<SHAHash>(digest);

    return hash;

}


ptr<partial_sha_hash> Transaction::getPartialHash() {


    auto hash = getHash();

    if (partialHash) {
        return partialHash;
    }

    partialHash = make_shared<partial_sha_hash>();

    for (size_t i = 0; i < PARTIAL_SHA_HASH_LEN; i++) {
        (*partialHash)[i] = hash->at(i);
    }

    return partialHash;

}

Transaction::Transaction(const ptr<vector<uint8_t>> data) : data(data) {

};

const ptr<vector<uint8_t>> &Transaction::getData() const {
    return data;
}

Transaction::~Transaction() {

}

