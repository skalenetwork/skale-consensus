//
// Created by kladko on 31.01.22.
//

#include "SkaleCommon.h"
#include "Log.h"

#include "BlockEncryptedArguments.h"

void BlockEncryptedArguments::add(uint64_t  _transactionIndex, ptr<EncryptedArgument> _argument) {
    CHECK_STATE(_argument);
    LOCK(m)

    CHECK_STATE2(arguments.count(_transactionIndex) == 0, "Argument exist for this transaction")
    arguments.emplace(_transactionIndex, _argument);
}

ptr<map<uint64_t, string>> BlockEncryptedArguments::getEncryptedTEKeys() {
    auto result = make_shared<map<uint64_t,string>>();

    LOCK(m)

    for (auto && argument: arguments) {
        auto ret = result->emplace(argument.first,argument.second->getEncryptedAesKey());
        CHECK_STATE(ret.second)
    }

    return result;
}
