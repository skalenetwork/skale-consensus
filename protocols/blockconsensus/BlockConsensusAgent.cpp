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
    @date 2018-
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"
#include "thirdparty/json.hpp"

#include "blockfinalize/client/BlockFinalizeDownloader.h"
#include "blockfinalize/client/BlockFinalizeDownloaderThreadPool.h"
#include "blockproposal/pusher/BlockProposalClientAgent.h"
#include "chains/Schain.h"
#include "crypto/BLAKE3Hash.h"
#include "crypto/ThresholdSigShare.h"
#include "crypto/CryptoManager.h"
#include "datastructures/BlockProposal.h"
#include "datastructures/BooleanProposalVector.h"
#include "datastructures/TransactionList.h"
#include "db/BlockDB.h"
#include "db/BlockProposalDB.h"
#include "db/BlockSigShareDB.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/InvalidStateException.h"
#include "messages/ConsensusProposalMessage.h"
#include "messages/InternalMessageEnvelope.h"
#include "messages/NetworkMessage.h"
#include "messages/NetworkMessageEnvelope.h"
#include "messages/ParentMessage.h"
#include "network/Network.h"
#include "node/Node.h"
#include "node/NodeInfo.h"
#include "pendingqueue/PendingTransactionsAgent.h"
#include "protocols/blockconsensus/BlockSignBroadcastMessage.h"
#include "thirdparty/lrucache.hpp"

#include "utils/Time.h"
#include "protocols/ProtocolKey.h"
#include  "protocols/binconsensus/BVBroadcastMessage.h"
#include  "protocols/binconsensus/BinConsensusInstance.h"

#include "protocols/binconsensus/ChildBVDecidedMessage.h"
#include "BlockConsensusAgent.h"
#include "datastructures/CommittedBlock.h"


BlockConsensusAgent::BlockConsensusAgent(Schain &_schain) : ProtocolInstance(
        BLOCK_SIGN, _schain) {
    trueDecisions = make_shared<cache::lru_cache<uint64_t, ptr<map<schain_index, ptr<ChildBVDecidedMessage>>>>>(
            MAX_CONSENSUS_HISTORY);
    falseDecisions = make_shared<cache::lru_cache<uint64_t, ptr<map<schain_index, ptr<ChildBVDecidedMessage>>>>>(
            MAX_CONSENSUS_HISTORY);
    decidedIndices = make_shared<cache::lru_cache<uint64_t, schain_index>>(MAX_CONSENSUS_HISTORY);


    BinConsensusInstance::initHistory(_schain.getNodeCount());


    for (int i = 0; i < _schain.getNodeCount(); i++) {
        children.push_back(make_shared<cache::lru_cache<uint64_t, ptr<BinConsensusInstance>>>(MAX_CONSENSUS_HISTORY));
    }

    auto blockDB = _schain.getNode()->getBlockDB();

    auto currentBlock = blockDB->readLastCommittedBlockID() + 1;

    for (int i = 0; i < _schain.getNodeCount(); i++) {
        children[i]->put((uint64_t) currentBlock, make_shared<BinConsensusInstance>(this, currentBlock, i + 1, true));
    }


};


void BlockConsensusAgent::startConsensusProposal(block_id _blockID, const ptr<BooleanProposalVector>& _proposal) {
    try {
        if (getSchain()->getLastCommittedBlockID() >= _blockID) {
            LOG(debug, "Terminating consensus proposal since already committed.");
        }

        LOG(debug, "CONSENSUS START:BLOCK:" + to_string(_blockID));


        for (uint64_t i = 1; i <= (uint64_t) getSchain()->getNodeCount(); i++) {
            if( getSchain()->getNode()->isExitRequested() )
                return;

            bin_consensus_value x;

            x = bin_consensus_value(_proposal->getProposalValue(schain_index(i)) ? 1 : 0);

            propose(x, schain_index(i), _blockID);
        }

    } catch (ExitRequestedException &) { throw; } catch (SkaleException &e) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


void BlockConsensusAgent::processChildMessageImpl(const ptr<InternalMessageEnvelope>& _me) {

    CHECK_ARGUMENT(_me);

    auto msg = dynamic_pointer_cast<ChildBVDecidedMessage>(_me->getMessage());

    reportConsensusAndDecideIfNeeded(msg);
}

void BlockConsensusAgent::propose(bin_consensus_value _proposal, schain_index _index, block_id _id) {

    try {

        auto key = make_shared<ProtocolKey>(_id, _index);

        auto child = getChild(key);

        auto msg = make_shared<BVBroadcastMessage>(_id, _index, bin_consensus_round(0), _proposal,
                Time::getCurrentTimeMs(), *child);


        auto id = (uint64_t) msg->getBlockId();

        CHECK_STATE(id != 0);

        child->processMessage(make_shared<InternalMessageEnvelope>(ORIGIN_PARENT, msg, *getSchain()));

    } catch (ExitRequestedException &) { throw; } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}


void BlockConsensusAgent::decideBlock(block_id _blockId, schain_index _sChainIndex, const string& _stats) {

    CHECK_ARGUMENT(!_stats.empty());

    try {

        BinConsensusInstance::logGlobalStats();


        LOG(info, string("BLOCK_DECIDE: PRPSR:") + to_string(_sChainIndex) +

                  ":BID:" + to_string(_blockId) + ":STATS:|" + _stats + "| Now signing block ...");


        auto msg = make_shared<BlockSignBroadcastMessage>(_blockId, _sChainIndex,
                Time::getCurrentTimeMs(), *this);

        auto signature = getSchain()->getNode()->getBlockSigShareDB()->checkAndSaveShareInMemory(msg->getSigShare(),
                                                                                         getSchain()->getCryptoManager());

        getSchain()->getNode()->getNetwork()->broadcastMessage(msg);

        decidedIndices->put((uint64_t) _blockId, _sChainIndex);

        if (signature != nullptr) {
            getSchain()->finalizeDecidedAndSignedBlock( _blockId, _sChainIndex, signature );
        }

    } catch (ExitRequestedException &) { throw; } catch (SkaleException &e) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}


void BlockConsensusAgent::decideDefaultBlock(block_id _blockNumber) {
    try {
        decideBlock(_blockNumber, schain_index(0), string("DEFAULT_BLOCK"));
    } catch (ExitRequestedException &) { throw; } catch (SkaleException &e) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


void BlockConsensusAgent::reportConsensusAndDecideIfNeeded(const ptr<ChildBVDecidedMessage>& _msg) {

    CHECK_ARGUMENT(_msg);

    try {

        auto nodeCount = (uint64_t) getSchain()->getNodeCount();
        schain_index blockProposerIndex = _msg->getBlockProposerIndex();
        auto blockID = _msg->getBlockId();

        if (blockID <= getSchain()->getLastCommittedBlockID()) {
            // Old consensus is reporting, already got this block through catchup
            return;
        }

        CHECK_STATE(blockProposerIndex <= nodeCount);

        if (decidedIndices->exists((uint64_t) blockID)) { return; }

        if (_msg->getValue()) {
            if (!trueDecisions->exists((uint64_t) blockID))
                trueDecisions->putIfDoesNotExist((uint64_t) blockID,
                                   make_shared<map<schain_index, ptr<ChildBVDecidedMessage>>>());

            auto map = trueDecisions->get((uint64_t) blockID);
            map->emplace(blockProposerIndex, _msg);

        } else {
            if (!falseDecisions->exists((uint64_t) blockID))
                falseDecisions->putIfDoesNotExist((uint64_t) blockID,
                                    make_shared<map<schain_index, ptr<ChildBVDecidedMessage>>>());

            auto map = falseDecisions->get((uint64_t) blockID);
            map->emplace(blockProposerIndex, _msg);
        }


        if (auto result = trueDecisions->getIfExists((uint64_t) blockID);
            !result.has_value() ||
            any_cast<ptr<map<schain_index, ptr<ChildBVDecidedMessage>>>>(result)->empty()) {

            if (auto result2 = falseDecisions->getIfExists((uint64_t) blockID);
                 result2.has_value() &&
                any_cast<ptr<map<schain_index, ptr<ChildBVDecidedMessage>>>>(result2)->size() == nodeCount) {
                decideDefaultBlock(blockID);
            }
            return;
        }

        uint64_t seed;

        if (blockID <= 1) {
            seed = 1;
        } else {

            CHECK_STATE(blockID - 1 <= getSchain()->getLastCommittedBlockID());
            auto previousBlock = getSchain()->getBlock(blockID - 1);
            if (previousBlock == nullptr)
                BOOST_THROW_EXCEPTION(InvalidStateException("Can not read block "
                                                            + to_string(blockID - 1) + " from LevelDB",
                                                            __CLASS_NAME__));
            seed = *((uint64_t *) previousBlock->getHash().data());
        }

        auto random = ((uint64_t) seed) % nodeCount;


        for (uint64_t i = random; i < random + nodeCount; i++) {
            auto index = schain_index(i % nodeCount) + 1;

            if (auto result = trueDecisions->getIfExists(((uint64_t) blockID));
                 result.has_value() &&
                any_cast<ptr<map<schain_index, ptr<ChildBVDecidedMessage>>>>(result)->count(index) > 0) {

                string statsString = buildStats(blockID);

                CHECK_STATE(!statsString.empty());

                decideBlock(blockID, index, statsString);

                return;
            }


            if (auto result = falseDecisions->getIfExists((uint64_t) blockID );!result.has_value() ||
                any_cast< ptr<map<schain_index, ptr<ChildBVDecidedMessage>>>>(
                     result)->count(index) == 0) {
                return;
            }
        }
    } catch (ExitRequestedException &) { throw; } catch (SkaleException &e) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}


void BlockConsensusAgent::processBlockSignMessage(const ptr<BlockSignBroadcastMessage>& _message) {
    try {
        auto signature =
                getSchain()->getNode()->getBlockSigShareDB()->checkAndSaveShareInMemory(_message->getSigShare(),
                                                                                getSchain()->getCryptoManager());
        if (signature == nullptr) {
            return;
        }


        auto proposer = _message->getBlockProposerIndex();
        auto blockId = _message->getBlockId();


        auto hash = BLAKE3Hash::getBlockHash(
            (uint64_t ) proposer,
            (uint64_t) blockId,
            (uint64_t) getSchain()->getSchainID());

        getSchain()->getCryptoManager()->verifyBlockSig(signature,
            hash);

        LOG(info, string("BLOCK_DECIDE (GOT SIG): PRPSR:") + to_string(proposer) +
                  ":BID:" + to_string(blockId) + "| Now signing block ...");


        getSchain()->finalizeDecidedAndSignedBlock(
            blockId, proposer, signature );

    } catch (ExitRequestedException &e) { throw; }
    catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

};


void BlockConsensusAgent::routeAndProcessMessage(const ptr<MessageEnvelope>& _me ) {

    CHECK_ARGUMENT( _me );

    try {


        CHECK_ARGUMENT( _me->getMessage()->getBlockId() > 0);
        CHECK_ARGUMENT( _me->getOrigin() != ORIGIN_PARENT);

        auto blockID = _me->getMessage()->getBlockId();

        // Future blockid messages shall never get to this point
        CHECK_ARGUMENT(blockID <= getSchain()->getLastCommittedBlockID() + 1);

        if (blockID + MAX_ACTIVE_CONSENSUSES < getSchain()->getLastCommittedBlockID())
            return; // message has a very old block id, ignore. They need to catchup

        if ( _me->getMessage()->getMessageType() == MSG_CONSENSUS_PROPOSAL) {

            auto consensusProposalMessage =
                dynamic_pointer_cast<ConsensusProposalMessage>( _me->getMessage());

            CHECK_STATE(fastMessageLedger);
            fastMessageLedger->writeProposalMessage(consensusProposalMessage);


            this->startConsensusProposal(
                _me->getMessage()->getBlockId(),
                                         consensusProposalMessage->getProposals());
            return;
        }

        if ( _me->getMessage()->getMessageType() == MSG_BLOCK_SIGN_BROADCAST) {

            auto blockSignBroadcastMessage = dynamic_pointer_cast<BlockSignBroadcastMessage>( _me->getMessage());


            CHECK_STATE(blockSignBroadcastMessage);

            fastMessageLedger->writeNetworkMessage(blockSignBroadcastMessage);

            this->processBlockSignMessage(dynamic_pointer_cast<BlockSignBroadcastMessage>( _me->getMessage()));
            return;
        }

        if ( _me->getOrigin() == ORIGIN_CHILD) {
            LOG(debug, "Got child message " + to_string( _me->getMessage()->getBlockId()) + ":" +
                       to_string( _me->getMessage()->getBlockProposerIndex()));

            auto internalMessageEnvelope = dynamic_pointer_cast<InternalMessageEnvelope>( _me );

            CHECK_STATE(internalMessageEnvelope);

            return processChildMessageImpl(internalMessageEnvelope);

        }


        ptr<ProtocolKey> key = _me->getMessage()->createDestinationProtocolKey();

        CHECK_STATE(key);

        {

            {

                auto child = getChild(key);

                if (child != nullptr) {
                    return child->processMessage( _me );
                }
            }
        }

    } catch (ExitRequestedException &) { throw; } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


bin_consensus_round BlockConsensusAgent::getRound(const ptr<ProtocolKey>& _key) {
    return getChild(_key)->getCurrentRound();
}

bool BlockConsensusAgent::decided(const ptr<ProtocolKey>& _key) {
    return getChild(_key)->decided();
}

ptr<BinConsensusInstance> BlockConsensusAgent::getChild(const ptr<ProtocolKey>& _key) {


    CHECK_ARGUMENT(_key);

    auto bpi = _key->getBlockProposerIndex();
    auto bid = _key->getBlockID();

    CHECK_ARGUMENT ((uint64_t) bpi <= (uint64_t) getSchain()->getNodeCount())

    try {

        LOCK(m)
        if (!children.at((uint64_t) bpi - 1)->exists((uint64_t) bid)) {
            children.at((uint64_t) bpi - 1)->putIfDoesNotExist(
                    (uint64_t) bid, make_shared<BinConsensusInstance>(this, bid, bpi));
        }

        return children.at((uint64_t) bpi - 1)->get((uint64_t) bid);

    } catch (ExitRequestedException &) { throw; } catch (SkaleException &e) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}


bool BlockConsensusAgent::shouldPost(const ptr<NetworkMessage>& _msg) {

    if (_msg->getMessageType() == MSG_BLOCK_SIGN_BROADCAST) {
        return true;
    }

    auto key = _msg->createDestinationProtocolKey();
    auto currentRound = getRound(key);
    auto r = _msg->getRound();

    if (r > currentRound + 1) { // way in the future
        return false;
    }

    if (r == currentRound + 1) { // if the previous round is decided, accept messages from the next round
        return decided(key);
    }


    return true;
}

string BlockConsensusAgent::buildStats(block_id _blockID) {

    ptr<map<schain_index, ptr<ChildBVDecidedMessage>>> tDecisions = nullptr;
    ptr<map<schain_index, ptr<ChildBVDecidedMessage>>> fDecisions = nullptr;


    if (auto result = trueDecisions->getIfExists((uint64_t) _blockID); result.has_value()) {
        tDecisions = any_cast<ptr<map<schain_index, ptr<ChildBVDecidedMessage>>>>(result);
    }

    if (auto result = falseDecisions->getIfExists((uint64_t) _blockID); result.has_value()) {
        fDecisions = any_cast<ptr<map<schain_index, ptr<ChildBVDecidedMessage>>>>(result);
    }

    string resultStr("");

    for (int i = 1; i <= getSchain()->getNodeCount(); i++) {
        string stats = to_string(i) + "|";
        ptr<ChildBVDecidedMessage> msg = nullptr;
        string decision;

        if (tDecisions && tDecisions->count(i) != 0) {
            msg = tDecisions->at(i);
            decision = "1";
        } else if (fDecisions && fDecisions->count(i) != 0) {
            decision = "0";
            msg = fDecisions->at(i);
        }

        if (msg != nullptr) {
            auto round = to_string(msg->getRound());
            auto processingTime = to_string(msg->getMaxProcessingTimeMs());
            auto latencyTime = to_string(msg->getMaxLatencyTimeMs());
            stats = stats + "D" + decision + "R" + round + "P" + processingTime +
                    "L" + latencyTime + "|";
        } else {
            stats += "*|";
        };

        resultStr.append(stats);
    }

    return resultStr;

}

ptr<vector<ptr<Message>>> BlockConsensusAgent::initFastLedgerAndReplayMessages(block_id _blockID) {

    LOG(info, "Initing fast message ledger with block ID:" + to_string((uint64_t) _blockID));

    fastMessageLedger = make_shared<FastMessageLedger>(getSchain(),
                                                       getSchain()->getNode()->getConsensusEngine()->getDbDir(), _blockID);

    auto msgs = fastMessageLedger->retrieveAndClearPreviosRunMessages();

    for (auto&& msg : *msgs) {

        ptr<MessageEnvelope> me;

        CHECK_STATE(getSchain());

        if (dynamic_pointer_cast<NetworkMessage>(msg) != nullptr) {

            auto nodeInfo = getSchain()->getNode()->getNodeInfoById(msg->getSrcNodeID());

            CHECK_STATE(nodeInfo);

            me = make_shared<NetworkMessageEnvelope>(
                    dynamic_pointer_cast<NetworkMessage>(msg),nodeInfo->getSchainIndex());
        } else {
            me = make_shared<InternalMessageEnvelope>(ORIGIN_EXTERNAL,
                                                      dynamic_pointer_cast<ConsensusProposalMessage>(msg), *getSchain());
        }

        getSchain()->postMessage(me);
    }

    LOG(info, "Inited fast message ledger with previous run messages:" + to_string(msgs->size()));

    return msgs;
}

void BlockConsensusAgent::startNewBlock(block_id _blockID) {
    CHECK_STATE(fastMessageLedger);
    fastMessageLedger->startNewBlock(_blockID);
}


