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

    @file DAProof.cpp
    @author Stan Kladko
    @date 2018
*/


#include "SkaleCommon.h"
#include "Log.h"
#include "utils/Time.h"
#include "exceptions/InvalidStateException.h"

#include "BlockProposal.h"
#include "DAProof.h"


DAProof::DAProof(const ptr<BlockProposal>& _proposal, ptr<ThresholdSignature>& _thresholdSignature) {
    CHECK_ARGUMENT( _proposal );
    CHECK_ARGUMENT(_thresholdSignature);

    this->thresholdSig = _thresholdSignature;
    this->schainID = _proposal->getSchainID();
    this->blockID = _proposal->getBlockID();
    this->proposerIndex = _proposal->getProposerIndex();
    this->proposerNodeID = _proposal->getProposerNodeID();
    this->hash = _proposal->getHash();
    this->creationTime = Time::getCurrentTimeMs();
}

schain_id DAProof::getSchainId() const {
    return schainID;
}

node_id DAProof::getProposerNodeId() const {
    return proposerNodeID;
}

block_id DAProof::getBlockId() const {
    return blockID;
}

schain_index DAProof::getProposerIndex() const {
    return proposerIndex;
}

ptr<ThresholdSignature> DAProof::getThresholdSig() const {
    CHECK_STATE(thresholdSig);
    return thresholdSig;
}

BLAKE3Hash DAProof::getHash() const {
    return hash;
}
uint64_t DAProof::getCreationTime() const {
    return creationTime;
}
