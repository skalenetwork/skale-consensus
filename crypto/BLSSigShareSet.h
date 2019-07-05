//
// Created by kladko on 7/5/19.
//

#ifndef SKALED_BLSSIGSHARESET_H
#define SKALED_BLSSIGSHARESET_H


#include <stdlib.h>
#include <string>
#include <mutex>

class BLSSigShareSet {
protected:
    size_t totalSigners;
    size_t requiredSigners;
    map<size_t, shared_ptr< BLSSigShare > > sigShares;

public:
    BLSSigShareSet( size_t requiredSigners, size_t totalSigners );
protected:
    recursive_mutex sigSharesMutex;



public:
    bool addSigShare( shared_ptr< BLSSigShare > _sigShare);

    unsigned long getTotalSigSharesCount();
    shared_ptr< BLSSigShare > getSigShareByIndex(size_t _index);
};



#endif  // SKALED_BLSSIGSHARESET_H
