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

    @file TimeoutAgen.cpp
    @author Stan Kladko
    @date 2018
*/


#include "SkaleCommon.h"
#include "Log.h"
#include "chains/Schain.h"
#include "OptimizerAgent.h"
#include "datastructures/CommittedBlock.h"

OptimizerAgent::OptimizerAgent(Schain &_sChain) : Agent(_sChain, false, true),
                                                  nodeCount(_sChain.getNodeCount()) {

}

bool OptimizerAgent::doOptimizedConsensus(block_id _blockId, uint64_t _lastBlockTimeStampS) {


    if (!getSchain()->fastConsensusPatch(_lastBlockTimeStampS)) {
        return false;
    }


    auto lastWinner = getLastWinner(_blockId);

    // if last time there was no winner (default block)
    // we do not optimize


    if (lastWinner == 0) {
        return false;
    }

    // redo full consensus each 17 blocks to
    // determine the winner. Othewise optimize
    return (uint64_t) _blockId % (nodeCount + 1) != 0;

}

schain_index OptimizerAgent::getLastWinner(block_id _blockId) {
    // first 16 blocks we do not know the winner
    if ((uint64_t) _blockId <= getSchain()->getNodeCount()) {
        return 0;
    }
    auto block = getSchain()->getBlock((uint64_t) _blockId - (uint64_t) getSchain()->getNodeCount());

    if (!block) {
        return 0;
    }

    return block->getProposerIndex();
}

schain_index OptimizerAgent::skipSendingProposalToTheNetwork(block_id _blockId) {
    // whe we run optimized consensus a node skips sending proposal to the network
    // if node chain index is not equal to the last winner
    return (getSchain()->getOptimizerAgent()->doOptimizedConsensus(_blockId,
                                                                   getSchain()->getLastCommittedBlockTimeStamp().getS()) &&
            (getSchain()->getOptimizerAgent()->getLastWinner(_blockId) != getSchain()->getSchainIndex()));
}


