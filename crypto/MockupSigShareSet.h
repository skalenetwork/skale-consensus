/*
    Copyright (C) 2019 SKALE Labs

    This file is part of skale-Mockup.

    skale-Mockup is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-Mockup is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with skale-Mockup.  If not, see <https://www.gnu.org/licenses/>.

    @file SigShareSet.h
    @author Stan Kladko
    @date 2019
*/

#pragma once

#include "datastructures/DataStructure.h"
#include "BLSSigShareSet.h"
#include "ThresholdSigShareSet.h"


class PartialHashesList;
class Schain;
class MockupSigShare;
class MockupSignature;
class SHAHash;

class MockupSigShareSet : public ThresholdSigShareSet {

    recursive_mutex m;

    bool wasMerged = false;

    std::map<size_t, ptr<MockupSigShare> > sigShares;

public:
    MockupSigShareSet(block_id _blockId, size_t _totalSigners, size_t _requiredSigners );

    ptr<ThresholdSignature> mergeSignature();

    bool addSigShare(const ptr<ThresholdSigShare>& _sigShare);

    bool isEnough();

    virtual ~MockupSigShareSet();


};
