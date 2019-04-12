/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skale-consensus.  If not, see <http://www.gnu.org/licenses/>.

    @file ReceivedBlockProposalsDatabase.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../../SkaleConfig.h"
#include "../../Agent.h"
#include "../../Log.h"
#include "../../exceptions/FatalError.h"
#include "../../thirdparty/json.hpp"
#include "../../abstracttcpserver/ConnectionStatus.h"
#include "leveldb/db.h"
#include "../../node/Node.h"
#include "../../chains/Schain.h"
#include "../../crypto/SHAHash.h"
#include "../../pendingqueue/PendingTransactionsAgent.h"
#include "../pusher/BlockProposalClientAgent.h"
#include "../../datastructures/BlockProposalSet.h"
#include "../../datastructures/BlockProposal.h"


#include "ReceivedBlockProposalsDatabase.h"


using namespace std;



ReceivedBlockProposalsDatabase::ReceivedBlockProposalsDatabase(Schain &_sChain) : Agent(_sChain, true){
    oldBlockID = _sChain.getBootstrapBlockID();
};


bool ReceivedBlockProposalsDatabase::addBlockProposal(ptr<BlockProposal> _proposal) {




    ASSERT(_proposal);

    LOG(trace, "addBlockProposal blockID_=" + to_string(_proposal->getBlockID()) + " proposerIndex=" +
               to_string(_proposal->getProposerIndex()));

    lock_guard<recursive_mutex> lock(proposalsDatabaseMutex);

    if (this->proposedBlockSets.count(_proposal->getBlockID()) == 0) {
        proposedBlockSets[_proposal->getBlockID()] = make_shared<BlockProposalSet>(this->sChain, _proposal->getBlockID());
    }

    proposedBlockSets[_proposal->getBlockID()]->addProposal(_proposal);


    return (proposedBlockSets[_proposal->getBlockID()]->isTwoThird());
}



void ReceivedBlockProposalsDatabase::cleanOldBlockProposals(block_id _lastCommittedBlockID) {

    lock_guard<recursive_mutex> lock(proposalsDatabaseMutex);

    if (_lastCommittedBlockID < BLOCK_PROPOSAL_HISTORY_SIZE)
        return;

    oldBlockID = _lastCommittedBlockID - BLOCK_PROPOSAL_HISTORY_SIZE;

    for (auto it = proposedBlockSets.cbegin(); it != proposedBlockSets.end();) {
        if (it->first  <= oldBlockID) {
            proposedBlockSets.erase(it++);
        } else{
            ++it;
        }
    }
}

ptr<vector<bool>> ReceivedBlockProposalsDatabase::getBooleanProposalsVector(block_id _blockID) {


    lock_guard<recursive_mutex> lock(proposalsDatabaseMutex);



    auto set = getProposedBlockSet((_blockID));

    ASSERT(set);

    return set->createBooleanVector();

}




ptr<BlockProposalSet> ReceivedBlockProposalsDatabase::getProposedBlockSet(block_id blockID) {


    lock_guard<recursive_mutex> lock(proposalsDatabaseMutex);

    if (proposedBlockSets.count(blockID) == 0) {
        proposedBlockSets[blockID] = make_shared<BlockProposalSet>(this->sChain, blockID);
    }

    return proposedBlockSets[blockID];
}


ptr<BlockProposal> ReceivedBlockProposalsDatabase::getBlockProposal(block_id blockID, schain_index proposerIndex) {


    lock_guard<recursive_mutex> lock(proposalsDatabaseMutex);

    auto set = getProposedBlockSet(blockID);

    if (!set) {
        return nullptr;
    }

    return set->getProposalByIndex(proposerIndex);
}





bool ReceivedBlockProposalsDatabase::isTwoThird(block_id _blockID) {


    lock_guard<recursive_mutex> lock(proposalsDatabaseMutex);


    if (proposedBlockSets.count(_blockID) > 0) {
        return proposedBlockSets[_blockID]->isTwoThird();
    } else {
        return false;
    };
}
