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

    @file OptimizerAgent.h
    @author Stan Kladko
    @date 2024 -
*/

#pragma once

class Agent;
class Schain;


class OptimizerAgent : public Agent {

    uint64_t nodeCount;

public:
    explicit OptimizerAgent( Schain& _sChain );


    // we determine consensus winner each 16 blocks
    bool doOptimizedConsensus(block_id _blockId);

    schain_index getLastWinner(block_id _block);


    schain_index skipSendingProposalToTheNetwork(block_id _blockId);
};
