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

    @file ReceivedBlockProposalsDatabase.h
    @author Stan Kladko
    @date 2018
*/

#pragma once


class BlockProposalSet;

class PartialHashesList;

class Schain;

class BooleanProposalVector;


class ReceivedBlockProposalsDB : Agent {

    block_id oldBlockID;

    map<block_id, ptr<BlockProposalSet>> proposedBlockSets;

public:

    ptr<BlockProposalSet> getProposedBlockSet(block_id _blockID);

    ptr<BlockProposal> getBlockProposal(block_id _blockID, schain_index _proposerIndex);

    ReceivedBlockProposalsDB(Schain &_sChain);

    void cleanOldBlockProposals(block_id _lastCommittedBlockID);

    bool addBlockProposal(ptr<BlockProposal> _proposal);

    ptr<BooleanProposalVector> getBooleanProposalsVector(block_id _blockID);

    bool isTwoThird(block_id _blockID);

    bool addDAProof(ptr<DAProof> _proof);
};



