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
    auto result = make_shared<BlockEncryptedAesKeys>();

    for (auto && argument: args) {
        result->add(argument.first, argument.second->getEncryptedAesKey());
    }

    return result;
}