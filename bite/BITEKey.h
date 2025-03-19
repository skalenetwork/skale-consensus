#pragma once

#include <SkaleCommon.h>


class BITEKey {
    uint64_t epoch;
    shared_ptr<vector<uint64_t> > encryptedKey;
};


