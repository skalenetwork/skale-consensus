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

    @file BinConsensusInstance.cpp
    @author Stan Kladko
    @date 2018
*/


#include "SkaleCommon.h"
#include "SkaleLog.h"
#include "blockproposal/pusher/BlockProposalClientAgent.h"
#include "chains/Schain.h"
#include "crypto/ConsensusBLSSigShare.h"
#include "crypto/ConsensusBLSSignature.h"
#include "crypto/ConsensusSigShareSet.h"
#include "crypto/CryptoManager.h"
#include "db/BlockProposalDB.h"
#include "db/ConsensusStateDB.h"
#include "db/RandomDB.h"
#include "exceptions/FatalError.h"
#include "messages/HistoryMessage.h"
#include "messages/InternalMessageEnvelope.h"
#include "messages/MessageEnvelope.h"
#include "messages/NetworkMessageEnvelope.h"
#include "messages/ParentMessage.h"
#include "network/Network.h"
#include "node/Node.h"
#include "node/NodeInfo.h"
#include "pendingqueue/PendingTransactionsAgent.h"
#include "protocols/ProtocolInstance.h"
#include "protocols/blockconsensus/BlockConsensusAgent.h"
#include "thirdparty/json.hpp"
#include "utils/Time.h"

#include "AUXBroadcastMessage.h"
#include "ChildBVDecidedMessage.h"
#include "BVBroadcastMessage.h"
#include "BinConsensusInstance.h"


using namespace std;


void BinConsensusInstance::processMessage(ptr<MessageEnvelope> _m) {

    CHECK_STATE(_m);




    CHECK_STATE(_m->getMessage()->getBlockID() == getBlockID());

    CHECK_STATE(_m->getMessage()->getBlockProposerIndex() == getBlockProposerIndex());


    auto msgType = _m->getMessage()->getMessageType();

    auto msgOrigin = _m->getOrigin();


    if (msgOrigin == ORIGIN_NETWORK) {

        if (msgType != MSG_BVB_BROADCAST && msgType != MSG_AUX_BROADCAST)
            return;

        processNetworkMessageImpl(dynamic_pointer_cast<NetworkMessageEnvelope>(_m));

        return;


    } else if (msgOrigin == ORIGIN_PARENT) {
        processParentProposal(dynamic_pointer_cast<InternalMessageEnvelope>(_m));
    }
}


void BinConsensusInstance::ifAlreadyDecidedSendDelayedEstimateForNextRound(bin_consensus_round _round) {
    if (isDecided && _round == getCurrentRound() + 1 && isTwoThird(totalAUXVotes(getCurrentRound()))) {
        proceedWithNewRound(decidedValue);
    }
}


void BinConsensusInstance::processNetworkMessageImpl(ptr<NetworkMessageEnvelope> _me) {

    CHECK_STATE(_me)

    updateStats(_me);

    auto message = dynamic_pointer_cast<NetworkMessage>(_me->getMessage());


    auto round = message->getRound();

    addToHistory(dynamic_pointer_cast<NetworkMessage>(_me->getMessage()));

    CHECK_STATE2(round <= getCurrentRound() + 1, to_string(round) + ":" +
    to_string(getCurrentRound()) + ":" + to_string(getBlockID())) ;



    if (_me->getMessage()->getMessageType() == MSG_BVB_BROADCAST) {


        auto m = dynamic_pointer_cast<BVBroadcastMessage>(_me->getMessage());


        ASSERT(m);

        bvbVote(_me);

        networkBroadcastValueIfThird(m);

        ifAlreadyDecidedSendDelayedEstimateForNextRound(m->getRound());

        commitValueIfTwoThirds(m);

    } else if (_me->getMessage()->getMessageType() == MSG_AUX_BROADCAST) {

        auto m = dynamic_pointer_cast<AUXBroadcastMessage>(_me->getMessage());
        ASSERT(m);
        auxVote(_me);

        if (m->getRound() == getCurrentRound())
            proceedWithCommonCoinIfAUXTwoThird(m->getRound());
    }


}

void BinConsensusInstance::updateStats(const ptr<NetworkMessageEnvelope> &_me) {

    auto processingTime = Time::getCurrentTimeMs() - _me->getArrivalTime();

    if (processingTime > maxProcessingTimeMs) {
        maxProcessingTimeMs = processingTime;
    }

    auto m = (NetworkMessage *) _me->getMessage().get();

    auto latencyTime = (int64_t) _me->getArrivalTime() - (int64_t) m->getTimeMs();
    if (latencyTime < 0)
        latencyTime = 0;

    if (maxLatencyTimeMs < (uint64_t) latencyTime) {
        maxLatencyTimeMs = (uint64_t) latencyTime;
    }
}

void BinConsensusInstance::processParentProposal(ptr<InternalMessageEnvelope> _me) {


    auto m = dynamic_pointer_cast<BVBroadcastMessage>(_me->getMessage());


    addToHistory(dynamic_pointer_cast<NetworkMessage>(m));


    ASSERT(m->getRound() == 0);


    setProposal(m->getRound(), m->getValue());


    networkBroadcastValue(m);



    addBVSelfVoteToHistory(m->getRound(), m->getValue());

    bvbVote(_me);

    commitValueIfTwoThirds(m);

}


void BinConsensusInstance::addToHistory(shared_ptr<NetworkMessage>
#ifdef CONSENSUS_DEBUG
                                        m
#endif
) {

#ifdef CONSENSUS_DEBUG

    ASSERT(m);

    ASSERT(msgHistory);

    lock_guard<recursive_mutex> lock(historyMutex);

    msgHistory->push_back(m);

    if (msgHistory->size() > MSG_HISTORY_SIZE) {
        msgHistory->pop_front();
    }

#endif
}


void BinConsensusInstance::addBVSelfVoteToHistory(bin_consensus_round _r, bin_consensus_value _v) {

    addToHistory(dynamic_pointer_cast<NetworkMessage>(make_shared<HistoryBVSelfVoteMessage>(_r, _v, *this)));

}

void BinConsensusInstance::addAUXSelfVoteToHistory(bin_consensus_round _r, bin_consensus_value _v) {

    addToHistory(dynamic_pointer_cast<NetworkMessage>(make_shared<HistoryAUXSelfVoteMessage>(_r, _v, *this)));

}


void BinConsensusInstance::addDecideToHistory(bin_consensus_round _r, bin_consensus_value _v) {


    addToHistory(dynamic_pointer_cast<NetworkMessage>(make_shared<HistoryDecideMessage>(_r, _v, *this)));

}


void BinConsensusInstance::addNextRoundToHistory(bin_consensus_round _r, bin_consensus_value _v) {

    addToHistory(dynamic_pointer_cast<NetworkMessage>(make_shared<HistoryNewRoundMessage>(_r, _v, *this)));

}


void BinConsensusInstance::addCommonCoinToHistory(bin_consensus_round _r, bin_consensus_value _v) {

    addToHistory(dynamic_pointer_cast<NetworkMessage>(make_shared<HistoryCommonCoinMessage>(_r, _v, *this)));

}


void BinConsensusInstance::bvbVote(ptr<MessageEnvelope> _me) {

    BVBroadcastMessage *m = (BVBroadcastMessage *) _me->getMessage().get();
    bin_consensus_round r = m->getRound();
    bin_consensus_value v = m->getValue();

    schain_index index = _me->getSrcNodeInfo()->getSchainIndex();

    getSchain()->getNode()->getConsensusStateDB()->writeBVBVote(getBlockID(),
                                                                getBlockProposerIndex(), r, index, v);


    if (v) {
        bvbTrueVotes[r].insert(index);
    } else {
        bvbFalseVotes[r].insert(index);
    }
}


void BinConsensusInstance::auxVote(ptr<MessageEnvelope> _me) {
    AUXBroadcastMessage *m = (AUXBroadcastMessage *) _me->getMessage().get();
    auto r = m->getRound();
    bin_consensus_value v = m->getValue();

    auto index = _me->getSrcNodeInfo()->getSchainIndex();

    auto sigShare = m->getSigShare();

    getSchain()->getNode()->getConsensusStateDB()->writeAUXVote(getBlockID(),
                                                                getBlockProposerIndex(), r, index, v,
                                                                sigShare->toString());


    if (v) {
        auxTrueVotes[r][index] = sigShare;
    } else {
        auxFalseVotes[r][index] = sigShare;
    }

}


uint64_t BinConsensusInstance::totalAUXVotes(bin_consensus_round r) {
    return auxTrueVotes[r].size() + auxFalseVotes[r].size();
}

void BinConsensusInstance::auxSelfVote(bin_consensus_round _r,
        bin_consensus_value _v, ptr<ThresholdSigShare> _sigShare) {
    ASSERT(_sigShare);

    addAUXSelfVoteToHistory(_r, _v);


    getSchain()->getNode()->getConsensusStateDB()->writeAUXVote(getBlockID(),
                                                                getBlockProposerIndex(), _r,
                                                                getSchain()->getSchainIndex(), _v,
                                                                _sigShare->toString());

    if (_v) {
        ASSERT(auxTrueVotes[_r].count(getSchain()->getSchainIndex()) == 0);
        auxTrueVotes[_r][getSchain()->getSchainIndex()] = _sigShare;
    } else {
        ASSERT(auxFalseVotes[_r].count(getSchain()->getSchainIndex()) == 0);
        auxFalseVotes[_r][getSchain()->getSchainIndex()] = _sigShare;
    }

}


node_count BinConsensusInstance::getBVBVoteCount(bin_consensus_value _v, bin_consensus_round _r) {
    return node_count(((_v ? bvbTrueVotes[_r] : bvbFalseVotes[_r]).size()));
}

node_count BinConsensusInstance::getAUXVoteCount(bin_consensus_value _v, bin_consensus_round _r) {
    return node_count(((_v ? auxTrueVotes[_r] : auxFalseVotes[_r]).size()));
}

bool BinConsensusInstance::isThird(node_count count) {
    return count * 3 > getSchain()->getNodeCount();
}


bool BinConsensusInstance::isTwoThird(node_count count) {
    return (uint64_t) count * 3 > 2 * getSchain()->getNodeCount();
}

bool BinConsensusInstance::isThirdVote(ptr<BVBroadcastMessage> _m) {
    auto voteCount = getBVBVoteCount(_m->getValue(), _m->getRound());
    return isThird(voteCount);
}


bool BinConsensusInstance::isTwoThirdVote(ptr<BVBroadcastMessage> _m) {
    return isTwoThird(getBVBVoteCount(_m->getValue(), _m->getRound()));
}

void BinConsensusInstance::insertValue(bin_consensus_round _r, bin_consensus_value _v) {
    getSchain()->getNode()->getConsensusStateDB()->writeBinValue(getBlockID(),
            getBlockProposerIndex(), _r, _v);
    binValues[_r].insert(_v);
}

void BinConsensusInstance::commitValueIfTwoThirds(ptr<BVBroadcastMessage> _m) {

    auto r = _m->getRound();
    auto v = _m->getValue();


    if (binValues[r].count(v))
        return;


    if (isTwoThirdVote(_m)) {
        bool didAUXBroadcast = binValues[r].size() > 0;

        insertValue(r, v);

        if (!didAUXBroadcast) {
            auxBroadcastValue(r, v);
        }

        if (r == getCurrentRound())
            proceedWithCommonCoinIfAUXTwoThird(r);

    }

}


void BinConsensusInstance::networkBroadcastValueIfThird(ptr<BVBroadcastMessage> _m) {
    if (isThirdVote(_m)) {
        networkBroadcastValue(_m);
    }
}

void BinConsensusInstance::networkBroadcastValue(ptr<BVBroadcastMessage> _m) {

    auto v = _m->getValue();
    auto r = _m->getRound();

    if (broadcastValues[r].count(v) > 0)
        return;

    auto newMsg = make_shared<BVBroadcastMessage>(_m->getBlockID(), _m->getBlockProposerIndex(), _m->getRound(),
                                                  _m->getValue(), Time::getCurrentTimeMs(),
                                                  *this);

    getSchain()->getNode()->getNetwork()->broadcastMessage(newMsg);

    broadcastValues[r].insert(bin_consensus_value(v == 1));
}


void BinConsensusInstance::auxBroadcastValue(bin_consensus_round _r, bin_consensus_value _v) {

    auto m = make_shared<AUXBroadcastMessage>(_r, _v, blockID, blockProposerIndex,
            Time::getCurrentTimeMs(), *this);

    auxSelfVote(_r, _v, m->getSigShare());

    getSchain()->getNode()->getNetwork()->broadcastMessage(m);

}


void BinConsensusInstance::proceedWithCommonCoinIfAUXTwoThird(bin_consensus_round _r) {

    if (decided())
        return;

    ASSERT(_r == getCurrentRound());

    uint64_t verifiedValuesSize = 0;

    bool hasTrue = false;
    bool hasFalse = false;

    if (binValues[_r].count(bin_consensus_value(true)) > 0 && auxTrueVotes[_r].size() > 0) {
        verifiedValuesSize += auxTrueVotes[_r].size();
        hasTrue = true;
    }

    if (binValues[_r].count(bin_consensus_value(false)) > 0 && auxFalseVotes[_r].size() > 0) {
        verifiedValuesSize += auxFalseVotes[_r].size();
        hasFalse = true;
    }

    if (isTwoThird(node_count(verifiedValuesSize))) {

        uint64_t random;

        if (getSchain()->getNode()->isBlsEnabled()) {
            random = this->calculateBLSRandom(_r);
        } else {
            srand((uint64_t) _r + (uint64_t) getBlockID() * 123456);
            random = rand();
        }

        auto randomDB = getSchain()->getNode()->getRandomDB();

        randomDB->writeRandom(getBlockID(), getBlockProposerIndex(),
                              _r, random);

        proceedWithCommonCoin(hasTrue, hasFalse, random);

    }

}


void BinConsensusInstance::proceedWithCommonCoin(bool _hasTrue, bool _hasFalse, uint64_t _random) {


    ASSERT(!isDecided);

    LOG(debug, "ROUND_COMPLETE:BLOCK:" + to_string(blockID) + ":ROUND:" + to_string(getCurrentRound()));

    bin_consensus_value random(_random % 2 == 0);

    addCommonCoinToHistory(getCurrentRound(), random);


    if (_hasTrue && _hasFalse) {
        LOG(debug, "NEW ROUND:BLOCK:" + to_string(blockID) + ":ROUND:" + to_string(getCurrentRound()));
        proceedWithNewRound(random);
        return;
    } else {

        bin_consensus_value v(_hasTrue);

        if (v == random) {
            LOG(debug, "DECIDED VALUE" + to_string(blockID) + ":ROUND:" + to_string(getCurrentRound()));
            decide(v);
        } else {
            LOG(debug, "NEW ROUND:BLOCK:" + to_string(blockID) + ":ROUND:" + to_string(getCurrentRound()));
            proceedWithNewRound(v);
        }
    }

}


void BinConsensusInstance::proceedWithNewRound(bin_consensus_value _value) {


    ASSERT(getCurrentRound() < 100);
    ASSERT(isTwoThird(totalAUXVotes(getCurrentRound())));

    setCurrentRound(getCurrentRound() + 1);

    setProposal(getCurrentRound(), _value);

    addNextRoundToHistory(getCurrentRound(), _value);

    auto m = make_shared<BVBroadcastMessage>(getBlockID(), getBlockProposerIndex(),
                                             getCurrentRound(), _value,
                                             Time::getCurrentTimeMs(), *this);

    ptr<MessageEnvelope> me = make_shared<MessageEnvelope>(ORIGIN_NETWORK, m, getSchain()->getThisNodeInfo());

    networkBroadcastValue(m);

    addBVSelfVoteToHistory(m->getRound(), m->getValue());
    bvbVote(me);

    commitValueIfTwoThirds(m);

}

void BinConsensusInstance::printHistory() {

#ifdef CONSENSUS_DEBUG
    cerr << "Proposer:" << getBlockProposerIndex() << "Nodecount:" << getNodeCount() << endl;
    for (auto &&m: *msgHistory) {

        if (m->getBlockProposerIndex() == getBlockProposerIndex() &&
            m->getBlockID() == getBlockID() && m->getDstNodeID() == getSchain()->getNode()->getNodeID()) {
            m->printMessage();
        }
    };
    cerr << endl;
#endif
}

void BinConsensusInstance::addDecideToGlobalHistory(bin_consensus_value _decidedValue) {

        lock_guard<recursive_mutex> lock(historyMutex);

        auto trueCache = globalTrueDecisions->at((uint64_t)getBlockProposerIndex() - 1);
        auto falseCache = globalFalseDecisions->at((uint64_t)getBlockProposerIndex() - 1);
        auto child = getSchain()->getBlockConsensusInstance()->getChild(
                this->getProtocolKey());

        CHECK_STATE(child);
        CHECK_STATE(trueCache);
        CHECK_STATE(falseCache);

        if (_decidedValue) {
            if (falseCache->exists((uint64_t ) getBlockID())) {
                printHistory();
                falseCache->get((uint64_t) getBlockID())->printHistory();
                ASSERT(false);
            }
            trueCache->put((uint64_t) getBlockID(), child);
        } else {
            if (trueCache->exists((uint64_t ) getBlockID())) {
                printHistory();
                trueCache->get((uint64_t) getBlockID())->printHistory();
                ASSERT(false);
            }
            falseCache->put((uint64_t) getBlockID(), child);
        }
}

void BinConsensusInstance::decide(bin_consensus_value _b) {

    ASSERT(!isDecided);

    setDecidedRoundAndValue(getCurrentRound(), bin_consensus_value(_b));

    addDecideToGlobalHistory(decidedValue);

    auto msg = make_shared<ChildBVDecidedMessage>((bool) _b, *this, this->getProtocolKey(),
                                                  this->getCurrentRound(), maxProcessingTimeMs,
                                                  maxLatencyTimeMs);


    LOG(debug, "Decided value: " + to_string(decidedValue) + " for blockid:" +
               to_string(getBlockID()) + " proposer:" +
               to_string(getBlockProposerIndex()));

    auto envelope = make_shared<InternalMessageEnvelope>(ORIGIN_CHILD, msg, *getSchain(), getProtocolKey());

    blockConsensusInstance->routeAndProcessMessage(envelope);

}


const block_id BinConsensusInstance::getBlockID() const {
    return blockID;
}

const schain_index BinConsensusInstance::getBlockProposerIndex() const {
    return blockProposerIndex;
}


BinConsensusInstance::BinConsensusInstance(BlockConsensusAgent *_instance, block_id _blockId,
                                           schain_index _blockProposerIndex, bool _initFromDB) :
        ProtocolInstance(BIN_CONSENSUS, *_instance->getSchain()) ,
        blockConsensusInstance(_instance), blockID(_blockId), blockProposerIndex(_blockProposerIndex),
        nodeCount(_instance ? _instance->getSchain()->getNodeCount() : 0),
        protocolKey(make_shared<ProtocolKey>(_blockId, _blockProposerIndex))
         {
    CHECK_ARGUMENT((uint64_t) _blockId > 0);
    CHECK_ARGUMENT((uint64_t) _blockProposerIndex > 0);
    CHECK_ARGUMENT(_instance);


    if (_initFromDB) {
        initFromDB(_instance);
    }
}

void BinConsensusInstance::initFromDB(const BlockConsensusAgent*
#ifdef CONSENSUS_STATE_PERSISTENCE
                                            _instance
#endif
                                                             ) {
#ifdef CONSENSUS_STATE_PERSISTENCE
    auto db = _instance->getSchain()->getNode()->getConsensusStateDB();

    currentRound = db->readCR(blockID, blockProposerIndex);
    auto result = db->readDR(blockID, blockProposerIndex);
    isDecided = result.first;

    if (isDecided) {
        decidedRound = result.second;
        decidedValue = db->readDV(blockID, blockProposerIndex);
    }

    auto bvVotes = db->readBVBVotes(blockID, blockProposerIndex);

    bvbTrueVotes.insert(bvVotes.first->begin(), bvVotes.first->end());
    bvbFalseVotes.insert(bvVotes.second->begin(), bvVotes.second->end());

    auto auxVotes = db->readAUXVotes(blockID, blockProposerIndex,
                                     _instance->getSchain()->getCryptoManager());

    auxTrueVotes.insert(auxVotes.first->begin(), auxVotes.first->end());
    auxFalseVotes.insert(auxVotes.first->begin(), auxVotes.first->end());

    auto bValues = db->readBinValues(blockID, blockProposerIndex);

    binValues.insert(bValues->begin(), bValues->end());

    auto props = db->readPRs(blockID, blockProposerIndex);

    proposals.insert(props->begin(), props->end());
#endif
}


void BinConsensusInstance::initHistory(node_count _nodeCount) {

    CHECK_ARGUMENT(_nodeCount > 0);

    // if not already inited, init

    if (globalTrueDecisions == nullptr) {
        globalTrueDecisions =
                make_shared<vector<ptr<cache::lru_cache<uint64_t, ptr<BinConsensusInstance>>>>>();
        globalFalseDecisions =
                make_shared<vector<ptr<cache::lru_cache<uint64_t, ptr<BinConsensusInstance>>>>>();

        for (uint64_t i = 0; i < (uint64_t) _nodeCount; i++) {
            globalTrueDecisions->
                    push_back(
                    make_shared<cache::lru_cache<uint64_t, ptr<BinConsensusInstance>>>(MAX_CONSENSUS_HISTORY));
            globalFalseDecisions->
                    push_back(
                    make_shared<cache::lru_cache<uint64_t, ptr<BinConsensusInstance>>>(MAX_CONSENSUS_HISTORY));
        }

#ifdef CONSENSUS_DEBUG
        msgHistory = make_shared<list<ptr<NetworkMessage>>>();
#endif
    }
}

bin_consensus_round BinConsensusInstance::getCurrentRound() {
    return currentRound;
}

void BinConsensusInstance::setCurrentRound(bin_consensus_round _currentRound) {
    currentRound = _currentRound;
    getSchain()->getNode()->getConsensusStateDB()->writeCR(getBlockID(),
                                                           blockProposerIndex, _currentRound);
}

bool BinConsensusInstance::decided() const {
    return isDecided;
}


ptr<vector<ptr<cache::lru_cache<uint64_t, ptr<BinConsensusInstance>>>>> BinConsensusInstance::globalTrueDecisions = nullptr;

ptr<vector<ptr<cache::lru_cache<uint64_t, ptr<BinConsensusInstance>>>>>  BinConsensusInstance::globalFalseDecisions = nullptr;

#ifdef CONSENSUS_DEBUG
ptr<list<ptr<NetworkMessage>>> BinConsensusInstance::msgHistory = nullptr;
#endif


const node_count &BinConsensusInstance::getNodeCount() const {
    return nodeCount;
}

uint64_t BinConsensusInstance::calculateBLSRandom(bin_consensus_round _r) {


    auto shares = getSchain()->getCryptoManager()->createSigShareSet(getBlockID());

    if (binValues[_r].count(bin_consensus_value(true)) > 0 && auxTrueVotes[_r].size() > 0) {
        for (auto &&item: auxTrueVotes[_r]) {
            ASSERT(item.second);
            shares->addSigShare(item.second);
            if (shares->isEnough())
                break;
        }
    }

    if (binValues[_r].count(bin_consensus_value(false)) > 0 && auxFalseVotes[_r].size() > 0) {
        for (auto &&item: auxFalseVotes[_r]) {
            ASSERT(item.second);
            shares->addSigShare(item.second);
            if (shares->isEnough())
                break;
        }
    }

    CHECK_STATE(shares->isEnough());

    auto random = shares->mergeSignature()->getRandom();

    LOG(debug, "Random for round: " + to_string(_r) + ":" + to_string(random));

    return random;
}

void BinConsensusInstance::setDecidedRoundAndValue(const bin_consensus_round &_decidedRound,
                                                   const bin_consensus_value &_decidedValue) {
    isDecided = true;
    getSchain()->getNode()->getConsensusStateDB()->writeDR(getBlockID(), blockProposerIndex, _decidedRound);
    getSchain()->getNode()->getConsensusStateDB()->writeDV(getBlockID(), blockProposerIndex, _decidedValue);
    decidedRound = _decidedRound;
    decidedValue = _decidedValue;

    addDecideToHistory(decidedRound, decidedValue);

}

void BinConsensusInstance::setProposal(bin_consensus_round _r, bin_consensus_value _v) {
    getSchain()->getNode()->getConsensusStateDB()->writePr(getBlockID(), blockProposerIndex,
                                                           _r, _v);
    proposals[_r] = _v;
}

recursive_mutex BinConsensusInstance::historyMutex;
