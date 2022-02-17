//
// Created by kladko on 01.02.22.
//



#ifndef SKALED_BLOCKENCRYPTEDARGUMENTS_H
#define SKALED_BLOCKENCRYPTEDARGUMENTS_H

class EncryptedTransactionAnalyzerInterface;

class BlockProposal;
class BlockEncryptedAesKeys;
class EncryptedArgument;

class BlockEncryptedArguments {

    map<uint64_t, ptr<EncryptedArgument>> args;

    ptr<BlockEncryptedAesKeys> cachedEncryptedKeys = nullptr;
    ptr<map<uint64_t, ptr<vector<uint8_t>>>> cachedEncryptedSegments = nullptr;

    recursive_mutex m;
public:

    uint64_t size();

    void insert(uint64_t _i, ptr<EncryptedArgument> _arg);

    ptr<BlockEncryptedAesKeys> getEncryptedAesKeys();

    ptr<map<uint64_t, ptr<vector<uint8_t>>>> getAesEncryptedSegments();

};


#endif //SKALED_BLOCKENCRYPTEDARGUMENTS_H
