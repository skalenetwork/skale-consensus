//
// Created by skale on 10/20/19.
//

#include "ListOfHashes.h"


SHAHash ListOfHashes::calculateMerkleRoot() {
    CHECK_STATE(size() > 0);
    return calculateMerkleRoot(0, size());
}
