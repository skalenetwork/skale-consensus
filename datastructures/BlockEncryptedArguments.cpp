//
// Created by kladko on 01.02.22.
//

#include "SkaleCommon.h"
#include "Log.h"

#include "datastructures/BlockProposal.h"
#include "datastructures/TransactionList.h"
#include "datastructures/Transaction.h"
#include "datastructures/BlockEncryptedAesKeys.h"

#include "crypto/EncryptedArgument.h"
#include "BlockEncryptedArguments.h"


void BlockEncryptedArguments::insert(uint64_t _i, ptr<EncryptedArgument> _arg) {
    LOCK(m);
    CHECK_STATE(_arg);
    CHECK_STATE(args.emplace(_i, _arg).second);
}

ptr<BlockEncryptedAesKeys> BlockEncryptedArguments::getEncryptedAesKeys() {

    LOCK(m)

    if (cachedEncryptedKeys != nullptr) {
        return cachedEncryptedKeys;
    }

    cachedEncryptedKeys = make_shared<BlockEncryptedAesKeys>();


    for (auto && argument: args) {
        cachedEncryptedKeys->add(argument.first, argument.second->getEncryptedAesKey());
    }

    return cachedEncryptedKeys;
}

ptr<map<uint64_t, ptr<vector<uint8_t>>>> BlockEncryptedArguments::getAesEncryptedSegments() {

    LOCK(m)

    if (cachedEncryptedSegments != nullptr) {
        return cachedEncryptedSegments;
    }

    cachedEncryptedSegments = make_shared<map<uint64_t, ptr<vector<uint8_t>>>>();

    for (auto && arg: args) {
        CHECK_STATE(cachedEncryptedSegments->emplace(arg.first, arg.second->getAesEncryptedSegment()).second)
    }

    return cachedEncryptedSegments;
}



uint64_t BlockEncryptedArguments::size() {
    LOCK(m)
    return this->args.size();
}
