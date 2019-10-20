//
// Created by skale on 10/20/19.
//

#ifndef SKALED_LISTOFHASHES_H
#define SKALED_LISTOFHASHES_H


class SHAHAsh;

class ListOfHashes {
    virtual uint64_t size() = 0;
    virtual ptr<SHAHash> getHash(uint64_t _index) = 0;
    ptr<SHAHash> calculateTopMerkleRoot();
    ptr<SHAHash> calculateMerkleRoot(uint64_t _startIndex, uint64_t _count);
};


#endif //SKALED_LISTOFHASHES_H
