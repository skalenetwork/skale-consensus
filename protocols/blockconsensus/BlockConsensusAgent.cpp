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

    @file BlockConsensusAgent.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../../SkaleCommon.h"
#include "../../Log.h"
#include "../../exceptions/FatalError.h"
#include "../../thirdparty/json.hpp"

#include "../../utils/Time.h"
#include "../../crypto/SHAHash.h"
#include "../../crypto/ThresholdSigShare.h"
#include "../../abstracttcpserver/ConnectionStatus.h"
#include "../../abstracttcpserver/ConnectionStatus.h"
#include "../../chains/Schain.h"
#include "../../node/Node.h"
#include "../../network/TransportNetwork.h"
#include "../../exceptions/ExitRequestedException.h"
#include "../../messages/ParentMessage.h"
#include "../../messages/InternalMessageEnvelope.h"
#include "../../messages/NetworkMessage.h"
#include "../../messages/NetworkMessageEnvelope.h"
#include "../../protocols/blockconsensus/BlockSignBroadcastMessage.h"
#include "../../pendingqueue/PendingTransactionsAgent.h"
#include "../../datastructures/BlockProposal.h"
#include "../../datastructures/ReceivedBlockProposal.h"
#include "../../datastructures/BlockProposalSet.h"
#include "../../datastructures/TransactionList.h"
#include "../../blockproposal/pusher/BlockProposalClientAgent.h"
#include "../../datastructures/BooleanProposalVector.h"
#include "../../messages/ConsensusProposalMessage.h"
#include "../../node/NodeInfo.h"
#include "../../blockproposal/received/ReceivedBlockProposalsDatabase.h"
#include "../../exceptions/InvalidStateException.h"
#include "../../db/BlockSigShareDB.h"
#include "../../blockfinalize/client/BlockFinalizeDownloader.h"
#include "../../blockfinalize/client/BlockFinalizeDownloaderThreadPool.h"

#include "../../protocols/ProtocolKey.h"
#include  "../../protocols/binconsensus/BVBroadcastMessage.h"
#include  "../../protocols/binconsensus/BinConsensusInstance.h"

#include "../binconsensus/ChildBVDecidedMessage.h"
#include "BlockConsensusAgent.h"
#include "../../datastructures/CommittedBlock.h"


BlockConsensusAgent::BlockConsensusAgent(Schain &_schain) : ProtocolInstance(
        BLOCK_SIGN, _schain) {
};


void BlockConsensusAgent::startConsensusProposal(block_id _blockID, ptr<BooleanProposalVector> _proposal) {

    ASSERT(proposedBlocks.count(_blockID) == 0);

    if (getSchain()->getLastCommittedBlockID() >= _blockID) {
        LOG(debug, "Terminating consensus proposal since already committed.");
    }

    LOG(debug, "CONSENSUS START:BLOCK:" + to_string(_blockID));

    proposedBlocks.insert(_blockID);


    uint64_t truthCount = 0;

    for (size_t i = 0; i < getSchain()->getNodeCount(); i++) {
        if (_proposal->getProposalValue(schain_index(i + 1)))
            truthCount++;
    }

    ASSERT(3 * truthCount > getSchain()->getNodeCount() * 2);


    for (uint64_t i = 1; i <= (uint64_t) getSchain()->getNodeCount(); i++) {

        bin_consensus_value x;

        x = bin_consensus_value(_proposal->getProposalValue(schain_index(i)) ? 1 : 0);

        propose(x, schain_index(i), _blockID);
    }


}


void BlockConsensusAgent::processChildMessageImpl(ptr<InternalMessageEnvelope> _me) {


    auto m = dynamic_pointer_cast<ChildBVDecidedMessage>(_me->getMessage());


    reportConsensusAndDecideIfNeeded(m);
}

void BlockConsensusAgent::propose(bin_consensus_value _proposal, schain_index _index, block_id _id) {


    auto _nodeID = getSchain()->getNode()->getNodeInfoByIndex(_index)->getNodeID();


    auto key = make_shared<ProtocolKey>(_id, _index);

    auto child = getChild(key);

    auto msg = make_shared<BVBroadcastMessage>(_nodeID, _id, _index, bin_consensus_round(0), _proposal, *child);


    auto id = (uint64_t) msg->getBlockId();
    ASSERT(id != 0);

    child->processParentProposal(make_shared<InternalMessageEnvelope>(ORIGIN_PARENT, msg, *getSchain()));

}


void BlockConsensusAgent::decideBlock(block_id _blockId, schain_index _sChainIndex) {


    auto msg = make_shared<BlockSignBroadcastMessage>(_blockId, _sChainIndex, *this);

    auto signature = getSchain()->getNode()->getBlockSigShareDb()->checkAndSaveShare(msg->getSigShare(),
                                                                    getSchain()->getCryptoManager());

    getSchain()->getNode()->getNetwork()->broadcastMessage(msg);

    ASSERT(decidedBlocks.count(_blockId) == 0);

    decidedBlocks[_blockId] = _sChainIndex;



    if (signature != nullptr) {
        getSchain()->decideBlock(_blockId, _sChainIndex, signature);
    }

}


void BlockConsensusAgent::decideEmptyBlock(block_id _blockNumber) {
    decideBlock(_blockNumber, schain_index(0));
}


void BlockConsensusAgent::reportConsensusAndDecideIfNeeded(ptr<ChildBVDecidedMessage> _msg) {

    try {

        auto nodeCount = (uint64_t) getSchain()->getNodeCount();
        auto blockProposerIndex = (uint64_t) _msg->getBlockProposerIndex();
        auto blockID = _msg->getBlockId();

        ASSERT(blockProposerIndex <= nodeCount);


        if (decidedBlocks.count(blockID) > 0)
            return;

        ptr<CommittedBlock> previousBlock = nullptr;

        if (blockID > 1) {
            previousBlock = getSchain()->getBlock(blockID - 1);
            if (previousBlock == nullptr) {
                LOG(err, "Cannot read block from blocks_XXX.db");
                return;
            }
        }


        if (_msg->getValue()) {
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
            auto index = schain_index(i % nodeCount) + 1;
            if (trueDecisions[blockID].count(index) > 0) {
                decideBlock(blockID, index);
                return;
            }
            if (falseDecisions[blockID].count(index) == 0) {
                return;
            }
        }
    } catch (ExitRequestedException &) { throw; } catch (Exception &e) {

        Exception::logNested(e);
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}


void BlockConsensusAgent::processChildCompletedMessage(ptr<InternalMessageEnvelope> _me) {
    disconnect(_me->getSrcProtocolKey());
};

void BlockConsensusAgent::processBlockSignMessage(ptr<BlockSignBroadcastMessage> _message) {

    try {

        auto signature =
                getSchain()->getNode()->getBlockSigShareDb()->checkAndSaveShare(_message->getSigShare(),
                                                                                getSchain()->getCryptoManager());


        if (signature == nullptr) {
            return;
        }

        getSchain()->decideBlock(_message->getBlockId(), _message->getBlockProposerIndex(), signature);

    } catch (ExitRequestedException& e) { throw;}
    catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

};


void BlockConsensusAgent::disconnect(ptr<ProtocolKey> _key) {


    lock_guard<recursive_mutex> lock(childrenMutex);

    if (children.count(_key) == 0)
        return;

    auto child = children[_key];

    children.erase(_key);

    ASSERT(completedInstancesByProtocolKey.count(_key) == 0);

    completedInstancesByProtocolKey[_key] = child->getOutcome();
}


void BlockConsensusAgent::routeAndProcessMessage(ptr<MessageEnvelope> m) {

    try {

        ASSERT(m->getMessage()->getBlockId() > 0);

        ASSERT(m->getOrigin() != ORIGIN_PARENT);


        if (m->getMessage()->getMessageType() == MSG_CONSENSUS_PROPOSAL) {
            this->startConsensusProposal(m->getMessage()->getBlockId(),
                                         ((ConsensusProposalMessage *) m->getMessage().get())->getProposals());
            return;
        }

        if (m->getMessage()->getMessageType() == MSG_BLOCK_SIGN_BROADCAST) {
            this->processBlockSignMessage(dynamic_pointer_cast<BlockSignBroadcastMessage>(m->getMessage()));
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


        ptr<ProtocolKey> key = m->getMessage()->createDestinationProtocolKey();

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

    } catch (ExitRequestedException &) { throw; } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


bin_consensus_round BlockConsensusAgent::getRound(ptr<ProtocolKey> key) {
    return getChild(key)->getCurrentRound();
}

bool BlockConsensusAgent::decided(ptr<ProtocolKey> key) {
    return getChild(key)->decided();
}

ptr<BinConsensusInstance> BlockConsensusAgent::getChild(ptr<ProtocolKey> key) {

    if ((uint64_t) key->getBlockProposerIndex() > (uint64_t) getSchain()->getNodeCount())
        return nullptr;


    lock_guard<recursive_mutex> lock(childrenMutex);

    if (children.count(key) == 0)
        children[key] = make_shared<BinConsensusInstance>(this, key->getBlockID(), key->getBlockProposerIndex());

    return children.at(key);

}


