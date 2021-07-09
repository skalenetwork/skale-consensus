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

#include "SendableItem.h"

class Schain;
class  BLAKE3Hash;
class DAProofHeader;

class CryptoManager;
class ThresholdSignature;
class SubmitDAProofRequestHeader;

class DAProof : public SendableItem {



    uint64_t creationTime;

public:
    uint64_t getCreationTime() const;

protected:

    schain_id schainID;
    node_id proposerNodeID;
    block_id blockID;
    schain_index proposerIndex;
    BLAKE3Hash hash;
    ptr<ThresholdSignature> thresholdSig;

public:

    DAProof(const ptr<BlockProposal>& _proposal, ptr<ThresholdSignature>& _thresholdSig);

    [[nodiscard]] schain_id getSchainId() const;

    [[nodiscard]] node_id getProposerNodeId() const;

    [[nodiscard]] block_id getBlockId() const;

    [[nodiscard]] schain_index getProposerIndex() const;

    [[nodiscard]] ptr<ThresholdSignature> getThresholdSig() const;

    [[nodiscard]] BLAKE3Hash getHash() const;
};

