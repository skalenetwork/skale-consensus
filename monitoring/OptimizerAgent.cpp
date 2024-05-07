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

OptimizerAgent::OptimizerAgent( Schain& _sChain )
    : Agent( _sChain, false, true ), nodeCount( _sChain.getNodeCount() ) {}

bool OptimizerAgent::doOptimizedConsensus( block_id _blockId, uint64_t _lastBlockTimeStampS ) {
    if ( !getSchain()->fastConsensusPatchEnabled( _lastBlockTimeStampS ) ) {
        return false;
    }

    auto lastWinner = getPreviousWinner( _blockId );

    // if last time there was no winner (default block)
    // we do not optimize


    if ( lastWinner == 0 ) {
        return false;
    }

    // redo full consensus each 65 blocks to
    // determine the winner. Otherwise optimize
    // potentially we could redo full consensus even more rarely, but
    // for now lets be conservative
    return ( uint64_t ) _blockId % ( 4 * nodeCount + 1 ) != 0;
}

// returns the priority leader for the current block numbered from 0 to N-1
// if binary consensus for  proposers end up returning one
// the winner is determined by priority, where the leader has the highest
// priority, leader + 1 has lower priority, leader + 2 has yet lower priority and so on
uint64_t OptimizerAgent::getPriorityLeaderForBlock( block_id& _blockID ) {
    uint64_t priorityLeader;
    uint64_t seed;


    if ( _blockID <= 1 ) {
        seed = 1;
    } else {
        CHECK_STATE( _blockID - 1 <= getSchain()->getLastCommittedBlockID() );
        auto previousBlock = getSchain()->getBlock( _blockID - 1 );
        if ( previousBlock == nullptr )
            BOOST_THROW_EXCEPTION( InvalidStateException(
                "Can not read block " + to_string( _blockID - 1 ) + " from LevelDB",
                __CLASS_NAME__ ) );
        seed = *( ( uint64_t* ) previousBlock->getHash().data() );
    }

    auto lastBlockStampS = getSchain()->getLastCommittedBlockTimeStamp().getS();

    if ( getSchain()->fastConsensusPatchEnabled( lastBlockStampS ) ) {
        // in the new scheme priority leader changes in Round Robin fashion for normal consensus
        // and is the previous block winner in the optimized consensus.
        // note that the priorityLeader variable goes from 0 to N-1
        // Thats why we need to subtract one because schain index goes from 1 to N
        if ( doOptimizedConsensus( _blockID, lastBlockStampS ) ) {
            priorityLeader =
                ( uint64_t ) getSchain()->getOptimizerAgent()->getPreviousWinner( _blockID ) - 1;
        } else {
            priorityLeader = ( ( uint64_t ) _blockID ) % nodeCount;
        }
    } else {
        // old scheme
        priorityLeader = ( ( uint64_t ) seed ) % nodeCount;
    }

    CHECK_STATE( priorityLeader < nodeCount );
    return priorityLeader;
}

// this function returns the schain_index previous winner proposer 16 blocks ago
// or zero if there was no winner  (default block)
schain_index OptimizerAgent::getPreviousWinner( block_id _blockId ) {
    // first 16 blocks we do not know the  previos winner so we do full consensus
    if ( ( uint64_t ) _blockId <= nodeCount ) {
        return 0;
    }

    auto block = getSchain()->getBlock( ( uint64_t ) _blockId - nodeCount );

    CHECK_STATE( block )

    return block->getProposerIndex();
}


// this function tells if the node needs to skip sending its proposal to the network
// this happens if the consensus is optimized consensus and the node is not the previous winner

bool OptimizerAgent::skipSendingProposalToTheNetwork( block_id _blockId ) {
    // whe we run optimized consensus a node skips sending proposal to the network
    // if node chain index is not equal to the last winner
    return (
        doOptimizedConsensus( _blockId, getSchain()->getLastCommittedBlockTimeStamp().getS() ) &&
        ( getPreviousWinner( _blockId ) != getSchain()->getSchainIndex() ) );
}
