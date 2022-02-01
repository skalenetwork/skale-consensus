//
// Created by kladko on 01.02.22.
//

#include "SkaleCommon.h"
#include "Log.h"

#include "datastructures/BlockProposal.h"
#include "datastructures/TransactionList.h"
#include "datastructures/Transaction.h"

#include "EncryptedArgument.h"
#include "BlockEncryptedArguments.h"


void BlockEncryptedArguments::insert(uint64_t _i, ptr<EncryptedArgument> _arg) {
    CHECK_STATE(_arg);
    CHECK_STATE(args.emplace(_i, _arg).second);
}

BlockEncryptedArguments::BlockEncryptedArguments(ptr<BlockProposal> _proposal,
                       ptr<EncryptedTransactionAnalyzer> _analyzer) {
    CHECK_STATE(_proposal);
    CHECK_STATE(_analyzer);

    auto transactions = _proposal->getTransactionList()->getItems();

    for (uint64_t i = 0; i < transactions->size(); i++) {
        auto transactionBytes = transactions->at(i)->getData();
        vector<uint8_t> encryptedArgument;
        auto rawEncryptedArg = _analyzer->getLastSmartContractArgument(*transactionBytes);

        if (rawEncryptedArg) {
            auto arg = make_shared<EncryptedArgument>(rawEncryptedArg);
            CHECK_STATE(args.emplace(i, arg).second);
        }
    }
}
