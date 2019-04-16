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
