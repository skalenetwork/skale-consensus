/*
    Copyright (C) 2019-Present SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with skale-consensus.  If not, see <https://www.gnu.org/licenses/>.

    @file ListOfHashes.h
    @author Stan Kladko
    @date 2019
*/

#ifndef SKALED_LISTOFHASHES_H
#define SKALED_LISTOFHASHES_H

#include "DataStructure.h"

class SHAHAsh;

class ListOfHashes : public DataStructure {
public:
    virtual uint64_t hashCount() = 0;

    virtual BLAKE3Hash getHash( uint64_t _index ) = 0;

    BLAKE3Hash calculateTopMerkleRoot();
};


#endif  // SKALED_LISTOFHASHES_H
