//
// Created by kladko on 01.02.22.
//

#ifndef SKALED_BLOCKDECRYPTEDARGUMENTS_H
#define SKALED_BLOCKDECRYPTEDARGUMENTS_H


class BlockDecryptedArguments {
    ptr<map<uint64_t, ptr<vector<uint8_t>>>> decryptedArgs;
public:
    BlockDecryptedArguments();
};


#endif //SKALED_BLOCKDECRYPTEDARGUMENTS_H
