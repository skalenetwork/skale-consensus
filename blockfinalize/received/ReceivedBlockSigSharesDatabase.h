//
// Created by kladko on 5/2/19.
//

#ifndef SKALED_RECEIVEDBLOCKSIGSHARESDATABASE_H
#define SKALED_RECEIVEDBLOCKSIGSHARESDATABASE_H

#include "../../crypto/ReceivedSigSharesDatabase.h"

class ReceivedBlockSigSharesDatabase : public  ReceivedSigSharesDatabase {
public:
    ReceivedBlockSigSharesDatabase(Schain &sChain);
};


#endif //SKALED_RECEIVEDBLOCKSIGSHARESDATABASE_H
