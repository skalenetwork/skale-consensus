#pragma once

#include "DataStructure.h"



class PartialHashesList;
class Schain;
class BLSSigShare;
class BLSSignature;
class SHAHash;

class SigShareSet : public DataStructure  {
    recursive_mutex sigSharesMutex;

    Schain* sChain;
    block_id blockId;

    map< schain_index, ptr< BLSSigShare > > sigShares;

public:
    node_count getTotalSigSharesCount();

    SigShareSet( Schain* subChain, block_id blockId );

    bool addSigShare(ptr<BLSSigShare> _sigShare);

    bool isTwoThird();

    bool isTwoThirdMinusOne();

    ptr<BLSSigShare > getSigShareByIndex(schain_index _index);

    ptr<BLSSignature> mergeSignature();

    static uint64_t getTotalObjects() {
        return totalObjects;
    }

    virtual ~SigShareSet();

private:

    static atomic<uint64_t>  totalObjects;
};
