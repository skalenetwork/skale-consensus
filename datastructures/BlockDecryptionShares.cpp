//
// Created by kladko on 26.01.22.
//

#include "SkaleCommon.h"
#include "Log.h"
#include "BlockAesKeyDecryptionShare.h"
#include "BlockDecryptionShares.h"

BlockDecryptionShares::BlockDecryptionShares() {
    shares = make_shared<map<uint64_t, ptr<BlockAesKeyDecryptionShares>>>();
}

void BlockDecryptionShares::addShare(uint64_t _index, ptr<BlockAesKeyDecryptionShares> _share) {
    CHECK_STATE(_share)
    LOCK(m)
    CHECK_STATE2(shares->count(_index) == 0, "Duplicate shares in BlockDecryption::addShare");
    shares->emplace(_index, _share);
}
