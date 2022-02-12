//
// Created by kladko on 26.01.22.
//

#ifndef SKALED_BLOCKDECRYPTIONSHARES_H
#define SKALED_BLOCKDECRYPTIONSHARES_H

#include "BlockAesKeyDecryptionShare.h"


class BlockDecryptionShares {

    recursive_mutex m;

    ptr<map<uint64_t, ptr<BlockAesKeyDecryptionShares>>> shares;

public:
    BlockDecryptionShares();

    void addShare(uint64_t _index, ptr<BlockAesKeyDecryptionShares> _share);

};


#endif //SKALED_BLOCKDECRYPTIONSHARES_H
