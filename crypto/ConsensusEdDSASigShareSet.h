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

    @file ConsensusEdDSASigShareSet.h
    @author Stan Kladko
    @date 2020
*/

#pragma once

#include "datastructures/DataStructure.h"
#include "ThresholdSigShareSet.h"


class PartialHashesList;
class Schain;
class ConsensusEdDSASigShare;
class ConsensusEdDSASignature;
class  BLAKE3Hash;

class ConsensusEdDSASigShareSet : public ThresholdSigShareSet {

    schain_id schainId;
    map<uint64_t, string> edDSASet; // thread-safe
    recursive_mutex edDSASetLock;

public:
    ConsensusEdDSASigShareSet(schain_id _schainId, block_id _blockId, size_t _totalSigners, size_t _requiredSigners );

    ptr<ThresholdSignature> mergeSignature() override;

    bool addSigShare(const ptr<ThresholdSigShare>& _sigShare) override;

    bool isEnough() override;

    ~ConsensusEdDSASigShareSet() override;


};
