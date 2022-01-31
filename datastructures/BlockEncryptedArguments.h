//
// Created by kladko on 31.01.22.
//

#ifndef SKALED_BLOCKENCRYPTEDARGUMENTS_H
#define SKALED_BLOCKENCRYPTEDARGUMENTS_H

#include "crypto/EncryptedArgument.h"

class BlockEncryptedArguments {
    map<uint64_t, ptr<EncryptedArgument>> arguments;

    recursive_mutex m;

public:

    void add(uint64_t _transactionIndex, ptr<EncryptedArgument> _argument);

    ptr<map<uint64_t, string>> getEncryptedTEKeys();

};


#endif //SKALED_BLOCKENCRYPTEDARGUMENTS_H
