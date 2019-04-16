#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../exceptions/InvalidArgumentException.h"

#include "../exceptions/NetworkProtocolException.h"
#include "PartialHashesList.h"

PartialHashesList::~PartialHashesList() {}

PartialHashesList::PartialHashesList(
        transaction_count _transactionCount, ptr<vector<uint8_t> > _partialHashes)
        : transactionCount(_transactionCount), partialHashes(_partialHashes) {}

PartialHashesList::PartialHashesList(transaction_count _transactionCount)
        : transactionCount(_transactionCount) {
    auto s = size_t(uint64_t(_transactionCount)) * PARTIAL_SHA_HASH_LEN;

    if (s > MAX_BUFFER_SIZE) {
        BOOST_THROW_EXCEPTION(InvalidArgumentException("Buffer size too large", __CLASS_NAME__));
    }
    partialHashes = make_shared<vector<uint8_t> >(s);
}

transaction_count PartialHashesList::getTransactionCount() const {
    return transactionCount;
}

ptr<vector<uint8_t> > PartialHashesList::getPartialHashes() const {
    return partialHashes;
}


ptr<partial_sha_hash> PartialHashesList::getPartialHash(uint64_t i) {
    if (i >= transactionCount) {
        BOOST_THROW_EXCEPTION(
                NetworkProtocolException("Index i is more than messageCount:" + to_string(i), __CLASS_NAME__));
    }
    auto hash = make_shared<array<uint8_t, PARTIAL_SHA_HASH_LEN> >();

    for (size_t j = 0; j < PARTIAL_SHA_HASH_LEN; j++) {
        (*hash)[j] = (*partialHashes)[PARTIAL_SHA_HASH_LEN * i + j];
    }

    return hash;
}
