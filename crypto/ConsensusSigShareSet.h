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

    @file SigShareSet.h
    @author Stan Kladko
    @date 2019
*/

#pragma once

#include "BLSSigShare.h"
#include "BLSSigShareSet.h"
#include "datastructures/DataStructure.h"
#include "ThresholdSigShareSet.h"


class PartialHashesList;
class Schain;
class ConsensusBLSSigShare;
class ConsensusBLSSignature;
class  BLAKE3Hash;

class ConsensusSigShareSet : public ThresholdSigShareSet {

    BLSSigShareSet blsSet;


public:
    ConsensusSigShareSet(block_id _blockId, size_t _totalSigners, size_t _requiredSigners );

    ptr<ThresholdSignature> mergeSignature() override;

    bool addSigShare(const ptr<ThresholdSigShare>& _sigShare) override;

    bool isEnough() override;

    bool isEnoughMinusOne();

    ~ConsensusSigShareSet() override;


};
