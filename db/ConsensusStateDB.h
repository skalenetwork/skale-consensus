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

    @file ConsensusStateDB.h
    @author Stan Kladko
    @date 2019
*/


#ifndef SKALED_CONSENSUS_STATE_DB_H
#define SKALED_CONSENSUS_STATE_DB_H


#include "CacheLevelDB.h"

class ConsensusStateDB : public CacheLevelDB {

    const string getFormatVersion();

    ptr<string> createCurrentRoundKey(block_id _blockId, schain_index _proposerIndex);

    ptr<string> createDecidedRoundKey(block_id _blockId, schain_index _proposerIndex);

    ptr<string> createDecidedValueKey(block_id _blockId, schain_index _proposerIndex);

    ptr<string> createProposalKey(block_id _blockId, schain_index _proposerIndex, bin_consensus_round _r);

    ptr<string> createBVBVoteKey(block_id _blockId, schain_index _proposerIndex, bin_consensus_round _r,
                                 schain_index _voterIndex, bin_consensus_value _v);

    ptr<string> createBinValueKey(block_id _blockId, schain_index _proposerIndex, bin_consensus_round _r,
                                 bin_consensus_value _v);



    ptr<string> createAUXVoteKey(block_id _blockId, schain_index _proposerIndex, bin_consensus_round _r,
                                 schain_index _voterIndex, bin_consensus_value _v);



public:

   ConsensusStateDB(Schain *_sChain, string &_dirName, string &_prefix, node_id _nodeId,
                           uint64_t _maxDBSize);




    void writeCR(block_id _blockId, schain_index _proposerIndex, bin_consensus_round _r);

    void writeDR(block_id _blockId, schain_index _proposerIndex, bin_consensus_round _r);

    void writeDV(block_id _blockId, schain_index _proposerIndex, bin_consensus_value _v);

    void writePr(block_id _blockId, schain_index _proposerIndex, bin_consensus_round _r, bin_consensus_value _v);

    void writeBVBVote(block_id _blockId, schain_index _proposerIndex, bin_consensus_round _r,
                      schain_index _voterIndex, bin_consensus_value _v);

    void writeAUXVote(block_id _blockId, schain_index _proposerIndex, bin_consensus_round _r, schain_index _voterIndex,
                      bin_consensus_value _v, ptr<string> _sigShare);

    void writeBinValue(block_id _blockId, schain_index _proposerIndex, bin_consensus_round _r,
                      bin_consensus_value _v);


    bin_consensus_round readCR(block_id _blockId, schain_index _proposerIndex);

    bin_consensus_round readDR(block_id _blockId, schain_index _proposerIndex);

    bin_consensus_value readDV(block_id _blockId, schain_index _proposerIndex);
};


#endif
