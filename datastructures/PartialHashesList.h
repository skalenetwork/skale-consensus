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

    PartialHashesList(transaction_count _transactionCount, ptr<vector<uint8_t>> partialHashes_);

    msg_len getLen() {
        return msg_len(partialHashes->size());

    }

    ptr<partial_sha_hash> getPartialHash(uint64_t i) ;

};



