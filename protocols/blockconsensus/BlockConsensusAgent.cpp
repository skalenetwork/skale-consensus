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

    @file BlockConsensusAgent.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../../SkaleCommon.h"
#include "../../Log.h"
#include "../../exceptions/FatalError.h"

#include "../../thirdparty/json.hpp"

#include "../../crypto/SHAHash.h"
#include "../../abstracttcpserver/ConnectionStatus.h"
#include "../../abstracttcpserver/ConnectionStatus.h"
#include "../../chains/Schain.h"
#include "../../node/Node.h"
#include "../../exceptions/ExitRequestedException.h"
#include "../../messages/ParentMessage.h"
#include "../../messages/InternalMessageEnvelope.h"
#include "../../messages/NetworkMessage.h"
#include "../../messages/NetworkMessageEnvelope.h"
#include "../../pendingqueue/PendingTransactionsAgent.h"
#include "../../datastructures/BlockProposal.h"
#include "../../datastructures/ReceivedBlockProposal.h"
#include "../../datastructures/BlockProposalSet.h"
#include "../../datastructures/TransactionList.h"
#include "../../blockproposal/pusher/BlockProposalClientAgent.h"
#include "../../messages/ConsensusProposalMessage.h"
#include "../../node/NodeInfo.h"
#include "../../blockproposal/received/ReceivedBlockProposalsDatabase.h"

#include "../../protocols/ProtocolKey.h"
#include  "../../protocols/binconsensus/BVBroadcastMessage.h"
#include  "../../protocols/binconsensus/BinConsensusInstance.h"

#include "../binconsensus/ChildBVDecidedMessage.h"
#include "BlockConsensusAgent.h"
#include "../../datastructures/CommittedBlock.h"


BlockConsensusAgent::BlockConsensusAgent(Schain &_schain) : sChain(_schain) {

};


void BlockConsensusAgent::processMessage(ptr <MessageEnvelope> _m) {
    try {

        if (_m->getOrigin() == ORIGIN_CHILD) {

            if (_m->getMessage()->getMessageType() == CHILD_COMPLETED) {

                processChildCompletedMessage(dynamic_pointer_cast<InternalMessageEnvelope>(_m));
            } else {
                processChildMessageImpl(dynamic_pointer_cast<InternalMessageEnvelope>(_m));
            }

        }
    } catch (...) {
        throw_with_nested(FatalError(__FUNCTION__, __CLASS_NAME__));
    }

}


void BlockConsensusAgent::startConsensusProposal(block_id _blockID, ptr <vector<bool>> _proposal) {


    ASSERT(_proposal->size() == getSchain()->getNodeCount());
    ASSERT(proposedBlocks.count(_blockID) == 0);

    if (getSchain()->getCommittedBlockID() >= _blockID) {
        LOG(debug, "Terminating consensus proposal since already committed.");
    }

    LOG(debug, "CONSENSUS START:BLOCK:" + to_string(_blockID));

    proposedBlocks.insert(_blockID);


    uint64_t truthCount = 0;

    for (size_t i = 0; i < _proposal->size(); i++) {
        if ((*_proposal)[i])
            truthCount++;
    }

    ASSERT(3 * truthCount > _proposal->size() * 2);


    for (uint64_t i = 0; i < (uint64_t) getSchain()->getNodeCount(); i++) {

        bin_consensus_value x;

        x = bin_consensus_value((*_proposal)[i] ? 1 : 0);

        propose(x, schain_index(i), _blockID);
    }


}


void BlockConsensusAgent::processChildMessageImpl(ptr <InternalMessageEnvelope> _me) {


    auto m = dynamic_pointer_cast<ChildBVDecidedMessage>(_me->getMessage());


    reportConsensusAndDecideIfNeeded(m);
}

void BlockConsensusAgent::propose(bin_consensus_value _proposal, schain_index _index, block_id _id) {


    auto _nodeID = getSchain()->getNode()->getNodeInfoByIndex(_index + 1)->getNodeID(); // XXXX


    auto key = make_shared<ProtocolKey>(_id, _index + 1);

    auto child = getChild(key);

    auto msg = make_shared<BVBroadcastMessage>(_nodeID, _id, _index, bin_consensus_round(0), _proposal,
                                               *child);


    auto id = (uint64_t) msg->getBlockId();
    ASSERT(id != 0);

    child->processParentProposal(make_shared<InternalMessageEnvelope>(ORIGIN_PARENT, msg, *getSchain()));

}


void BlockConsensusAgent::decideBlock(block_id _blockNumber, schain_index _proposerIndex) {


    ASSERT(decidedBlocks.count(_blockNumber) == 0);

    decidedBlocks[_blockNumber] = _proposerIndex;

    LOG(debug, "decideBlock:" + to_string(_blockNumber) +
              ":PRP:" + to_string(_proposerIndex));
    LOG(debug, "Total txs:" + to_string(getSchain()->getTotalTransactions()) +
              " T(s):" +
              to_string((getSchain()->getCurrentTimeMilllis().count() - getSchain()->getStartTime().count()) / 1000.0));


    auto proposedBlockSet = getSchain()->blockProposalsDatabase->getProposedBlockSet(_blockNumber);


    ASSERT(proposedBlockSet);


    if (_proposerIndex == (uint64_t) getSchain()->getNodeCount()) {


        uint64_t time = Schain::getCurrentTimeMs();
        auto sec = time / 1000;

        auto ms = (uint32_t ) time % 1000;

        // empty block
        auto emptyList = make_shared<TransactionList>(make_shared < vector < ptr < Transaction >> > ());
        auto zeroProposal = make_shared<ReceivedBlockProposal>(*getSchain(), _blockNumber, _proposerIndex,
                                                               emptyList, sec, ms );


        //TODO: FIX TIME FOR EMPTY PROPOSAL!!!
        proposedBlockSet->addProposal(zeroProposal);
    }


    do {

        if (getSchain()->getNode()->isExitRequested()) {
            throw ExitRequestedException();
        }
        usleep(100000); /* Flawfinder: ignore */
    } while (proposedBlockSet->getProposalByIndex(_proposerIndex + 1) == nullptr); // XXXX

    auto proposal = proposedBlockSet->getProposalByIndex(_proposerIndex + 1); // XXXX

    getSchain()->blockCommitArrived(false, _blockNumber, _proposerIndex, proposal->getTimeStamp());

}


void BlockConsensusAgent::decideEmptyBlock(block_id blockNumber) {

    decideBlock(blockNumber, schain_index((uint64_t) getSchain()->getNodeCount() + 1));
}


void BlockConsensusAgent::reportConsensusAndDecideIfNeeded(ptr <ChildBVDecidedMessage> msg) {

    auto nodeCount = (uint64_t) getSchain()->getNodeCount();
    auto blockProposerIndex = (uint64_t) msg->getBlockProposerIndex();
    auto blockID = msg->getBlockId();

    ASSERT(blockProposerIndex < nodeCount);


    if (decidedBlocks.count(blockID) > 0)
        return;

    ptr <CommittedBlock> previousBlock = nullptr;

    if (blockID > 1) {
        previousBlock = getSchain()->getBlock(blockID - 1);
        if (previousBlock == nullptr) {
            return;
        }
    }


    if (msg->getValue()) {
        trueDecisions[blockID].insert(blockProposerIndex);
    } else {
        falseDecisions[blockID].insert(blockProposerIndex);
    }


    if (trueDecisions[blockID].size() == 0) {
        if ((uint64_t) falseDecisions[blockID].size() == nodeCount) {
            decideEmptyBlock(blockID);
        }
        return;
    }


    uint64_t seed;

    if (blockID <= 1) {
        seed = 1;
    } else {
        seed = *((uint64_t *) previousBlock->getHash()->data());
    }

    auto random = ((uint64_t) seed) % nodeCount;


    for (uint64_t i = random; i < random + nodeCount; i++) {
        auto index = schain_index(i % nodeCount);
        if (trueDecisions[blockID].count(index) > 0) {
            decideBlock(blockID, index);
            return;
        }
        if (falseDecisions[blockID].count(index) == 0) {
            return;
        }
    }


}


void BlockConsensusAgent::voteAndDecideIfNeded1(ptr <ChildBVDecidedMessage> msg) {

    auto nodeCount = (uint64_t) getSchain()->getNodeCount();
    auto blockProposerIndex = (uint64_t) msg->getBlockProposerIndex();
    auto blockID = msg->getBlockId();


    ASSERT(blockProposerIndex < nodeCount);


    if (decidedBlocks.count(blockID) > 0)
        return;

    if (msg->getValue()) {
        trueDecisions[blockID].insert(blockProposerIndex);
    } else {
        falseDecisions[blockID].insert(blockProposerIndex);
    }

    if (trueDecisions[blockID].size() == 0) {
        if ((uint64_t) falseDecisions[blockID].size() == nodeCount) {
            decideEmptyBlock(blockID);
        }
        return;
    }


    auto winner = ((uint64_t) blockID) % nodeCount;


    for (uint64_t i = winner; i < winner + nodeCount; i++) {
        auto index = schain_index(i % nodeCount);
        if (trueDecisions[blockID].count(index) > 0) {
            decideBlock(blockID, index);
            return;
        }
        if (falseDecisions[blockID].count(index) == 0) {
            return;
        }
    }


}


void BlockConsensusAgent::processChildCompletedMessage(ptr <InternalMessageEnvelope> _me) {
    disconnect(_me->getSrcProtocolKey());
};


void BlockConsensusAgent::disconnect(ptr <ProtocolKey> instanceKey) {


    lock_guard <recursive_mutex> lock(childrenMutex);

    if (children.count(instanceKey) == 0)
        return;

    auto child = children[instanceKey];

    children.erase(instanceKey);

    ASSERT(completedInstancesByProtocolKey.count(instanceKey) == 0);

    completedInstancesByProtocolKey[instanceKey] = child->getOutcome();
}


void BlockConsensusAgent::routeAndProcessMessage(ptr <MessageEnvelope> m) {


    ASSERT(m->getMessage()->getBlockId() > 0);

    ASSERT(m->getOrigin() != ORIGIN_PARENT);


    if (m->getMessage()->getMessageType() == MSG_CONSENSUS_PROPOSAL) {

        this->startConsensusProposal(m->getMessage()->getBlockId(),
                                     ((ConsensusProposalMessage *) m->getMessage().get())->getProposals());

        return;

    }


    if (m->getOrigin() == ORIGIN_CHILD) {
        LOG(debug, "Got child message " + to_string(m->getMessage()->getBlockId()) + ":" +
                   to_string(m->getMessage()->getBlockProposerIndex()));


        if (m->getMessage()->getMessageType() == CHILD_COMPLETED) {

            return processChildCompletedMessage(dynamic_pointer_cast<InternalMessageEnvelope>(m));
        } else {
            return processChildMessageImpl(dynamic_pointer_cast<InternalMessageEnvelope>(m));
        }

    }


    ptr <ProtocolKey> key = m->getMessage()->createDestinationProtocolKey();

    {

        {

            if (completedInstancesByProtocolKey.count((key))) {
                return;
            }

            auto child = getChild(key);

            if (child != nullptr) {
                return child->processMessage(m);
            }
        }
    }

}


Schain *BlockConsensusAgent::getSchain() const {
    return &sChain;
}

bin_consensus_round BlockConsensusAgent::getRound(ptr <ProtocolKey> key) {
    return getChild(key)->getCurrentRound();
}

bool BlockConsensusAgent::decided(ptr <ProtocolKey> key) {
    return getChild(key)->decided();
}

ptr <BinConsensusInstance> BlockConsensusAgent::getChild(ptr <ProtocolKey> key) {

    if ((uint64_t) key->getBlockProposerIndex() -1 >= (uint64_t) getSchain()->getNodeCount()) // XXXX
        return nullptr;


    lock_guard <recursive_mutex> lock(childrenMutex);

    if (children.count(key) == 0)
        children[key] = make_shared<BinConsensusInstance>(this, key->getBlockID(), key->getBlockProposerIndex() - 1); // XXXX

    return children[key];

}


