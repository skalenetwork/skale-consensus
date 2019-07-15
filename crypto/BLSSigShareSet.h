/*
    Copyright (C) 2019 SKALE Labs

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

    @file BLSSigShareSet.h
    @author Stan Kladko
    @date 2019
*/


#ifndef SKALED_BLSSIGSHARESET_H
#define SKALED_BLSSIGSHARESET_H


#include <stdlib.h>
#include <mutex>
#include <string>

class BLSSignature;

class BLSSigShareSet {

    size_t totalSigners;
    size_t requiredSigners;

    recursive_mutex sigSharesMutex;


    map<size_t, shared_ptr< BLSSigShare > > sigShares;

public:

    BLSSigShareSet( size_t requiredSigners, size_t totalSigners );

    bool isEnough();

    bool addSigShare( shared_ptr< BLSSigShare > _sigShare);

    unsigned long getTotalSigSharesCount();
    shared_ptr< BLSSigShare > getSigShareByIndex(size_t _index);
    shared_ptr<BLSSignature> merge();
};



#endif  // SKALED_BLSSIGSHARESET_H-
