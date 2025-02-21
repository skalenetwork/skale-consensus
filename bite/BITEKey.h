//
// Created by kladko on 21-02-2025.
//

#ifndef SKALED_BITEKEY_H
#define SKALED_BITEKEY_H


#include <SkaleCommon.h>


class BITEKey {
    uint64_t epoch;
    shared_ptr<vector<uint64_t> > encryptedKey;
};

#endif //SKALED_BITEKEY_H
