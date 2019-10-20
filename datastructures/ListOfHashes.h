//
// Created by skale on 10/20/19.
//

#ifndef SKALED_LISTOFHASHES_H
#define SKALED_LISTOFHASHES_H


class ListOfHashes {
    virtual uint64_t size() = 0;
    virtual uint64_t getHash(uint64_t _index) = 0;

    SHAHash calculateMerkleRoot();
    SHAHash calculateMerkleRoot(uint64_t _startIndex, uint64_t _endIndex);
};


#endif //SKALED_LISTOFHASHES_H
