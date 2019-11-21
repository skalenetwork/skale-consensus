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

    @file ReceivedBlockProposalsDatabase.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../../SkaleCommon.h"
#include "../../Agent.h"
#include "../../Log.h"
#include "../../exceptions/FatalError.h"
#include "../../exceptions/ExitRequestedException.h"
#include "../../thirdparty/json.hpp"
#include "../../abstracttcpserver/ConnectionStatus.h"
#include "leveldb/db.h"
#include "../../node/Node.h"
#include "../../chains/Schain.h"
#include "../../crypto/SHAHash.h"
#include "../../datastructures/DAProof.h"
#include "../../pendingqueue/PendingTransactionsAgent.h"
#include "../pusher/BlockProposalClientAgent.h"
#include "../../datastructures/BlockProposal.h"
#include "../../datastructures/BlockProposalSet.h"


#include "ReceivedBlockProposalsDatabase.h"


using namespace std;


ReceivedBlockProposalsDatabase::ReceivedBlockProposalsDatabase(Schain &_sChain) : Agent(_sChain, true) {
    try {
        oldBlockID = _sChain.getBootstrapBlockID();
    } catch (ExitRequestedException &) { throw; } catch (...) {
        throw_with_nested(FatalError(__FUNCTION__, __CLASS_NAME__));
    }
};

bool ReceivedBlockProposalsDatabase::addDAProof(ptr<DAProof> _proof) {

    LOCK(m)

    auto set = getProposedBlockSet(_proof->getBlockId());

    CHECK_STATE(set != nullptr);

    return set->addDAProof(_proof);
}


bool ReceivedBlockProposalsDatabase::addBlockProposal(ptr<BlockProposal> _proposal) {


    ASSERT(_proposal);
    CHECK_ARGUMENT(_proposal->getSignature() != nullptr);


    LOG(trace, "addBlockProposal blockID_=" + to_string(_proposal->getBlockID()) + " proposerIndex=" +
               to_string(_proposal->getProposerIndex()));

    LOCK(m)

    if (this->proposedBlockSets.count(_proposal->getBlockID()) == 0) {
        proposedBlockSets[_proposal->getBlockID()] = make_shared<BlockProposalSet>(this->sChain,
                                                                                   _proposal->getBlockID());
    }

    proposedBlockSets.at(_proposal->getBlockID())->add(_proposal);


    return proposedBlockSets.at(_proposal->getBlockID())->isTwoThird();
}


void ReceivedBlockProposalsDatabase::cleanOldBlockProposals(block_id _lastCommittedBlockID) {

    LOCK(m)

    if (_lastCommittedBlockID < BLOCK_PROPOSAL_HISTORY_SIZE)
        return;

    oldBlockID = _lastCommittedBlockID - BLOCK_PROPOSAL_HISTORY_SIZE;

    for (auto it = proposedBlockSets.cbegin(); it != proposedBlockSets.end();) {
        if (it->first <= oldBlockID) {
            proposedBlockSets.erase(it++);
        } else {
            ++it;
        }
    }
}

ptr<BooleanProposalVector> ReceivedBlockProposalsDatabase::getBooleanProposalsVector(block_id _blockID) {


    LOCK(m)

    auto set = getProposedBlockSet((_blockID));

    ASSERT(set);

    return set->createBooleanVector();

}


ptr<BlockProposalSet> ReceivedBlockProposalsDatabase::getProposedBlockSet(block_id _blockID) {

    LOCK(m)

    if (proposedBlockSets.count(_blockID) == 0) {
        proposedBlockSets[_blockID] = make_shared<BlockProposalSet>(this->sChain, _blockID);
    }

    return proposedBlockSets.at(_blockID);
}


ptr<BlockProposal> ReceivedBlockProposalsDatabase::getBlockProposal(block_id _blockID, schain_index _proposerIndex) {


    LOCK(m)

    auto set = getProposedBlockSet(_blockID);

    if (!set) {
        return nullptr;
    }

    auto proposal = set->getProposalByIndex(_proposerIndex);

    if (proposal == nullptr)
        return nullptr;

    CHECK_STATE(proposal->getSignature() != nullptr);

    return proposal;
}


bool ReceivedBlockProposalsDatabase::isTwoThird(block_id _blockID) {


    LOCK(m)

    if (proposedBlockSets.count(_blockID) > 0) {
        return proposedBlockSets.at(_blockID)->isTwoThird();
    } else {
        return false;
    };
}
