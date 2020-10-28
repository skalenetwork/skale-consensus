/*
    Copyright (C) 2018-2019 SKALE Labs

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

    @file DAProof.h
    @author Stan Kladko
    @date 2018
*/

#pragma  once

#include "SkaleCommon.h"

#include "DataStructure.h"

class Schain;
class  BLAKE3Hash;
class DAProofHeader;

class CryptoManager;
class ThresholdSignature;
class SubmitDAProofRequestHeader;

class DAProof : public DataStructure {


protected:

    schain_id schainID;
    node_id proposerNodeID;
    block_id blockID;
    schain_index proposerIndex;
    ptr<BLAKE3Hash>hash = nullptr;
    ptr<ThresholdSignature> thresholdSig = nullptr;

public:

    DAProof(const ptr<BlockProposal>& _proposal, ptr<ThresholdSignature>& _thresholdSig);

    schain_id getSchainId() const;

    node_id getProposerNodeId() const;

    block_id getBlockId() const;

    schain_index getProposerIndex() const;

    ptr<ThresholdSignature> getThresholdSig() const;

    ptr<BLAKE3Hash>getHash() const;
};

