/*
    Copyright (C) 2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skale-consensus.  If not, see <http://www.gnu.org/licenses/>.

    @file SigShareSet.h
    @author Stan Kladko
    @date 2019
*/

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

    SigShareSet( Schain* _sChain, block_id _blockId );

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
