//
// Created by kladko on 26.01.22.
//

#ifndef SKALED_BLOCKDECRYPTIONSHARES_H
#define SKALED_BLOCKDECRYPTIONSHARES_H

#include "ArgumentDecryptionShare.h"


class BlockDecryptionShares {

    recursive_mutex m;

    ptr<map<uint64_t, ptr<ArgumentDecryptionShare>>> shares;

public:
    BlockDecryptionShares();

    void addShare(uint64_t _index, ptr<ArgumentDecryptionShare> _share);

};


#endif //SKALED_BLOCKDECRYPTIONSHARES_H
