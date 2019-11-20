//
// Created by skale on 10/20/19.
//

#ifndef SKALED_LISTOFHASHES_H
#define SKALED_LISTOFHASHES_H

#include "DataStructure.h"

class SHAHAsh;

class ListOfHashes : public DataStructure {

public:

    virtual uint64_t hashCount() = 0;
    virtual ptr<SHAHash> getHash(uint64_t _index) = 0;
    ptr<SHAHash> calculateTopMerkleRoot();
};


#endif //SKALED_LISTOFHASHES_H
