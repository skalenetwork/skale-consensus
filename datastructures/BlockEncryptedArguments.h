//
// Created by kladko on 01.02.22.
//



#ifndef SKALED_BLOCKENCRYPTEDARGUMENTS_H
#define SKALED_BLOCKENCRYPTEDARGUMENTS_H

class EncryptedTransactionAnalyzerInterface;

class BlockProposal;

class BlockEncryptedArguments {
    map<uint64_t, ptr<EncryptedArgument>> args;

    recursive_mutex m;
public:

    void insert(uint64_t _i, ptr<EncryptedArgument> _arg);

    ptr<map<uint64_t, string>> getEncryptedTEKeys();

};


#endif //SKALED_BLOCKENCRYPTEDARGUMENTS_H
