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

/*
 *     Note: comments refer to
 *    Signature-Free Asynchronous Byzantine Consensus with < n/3 and O(n2)) Messages
 *    Achour Mostefaoui, Moumen Hamouna, Michel Raynal
 *
 *    https://hal.inria.fr/hal-00944019v2/document
 *
 */


#include "SkaleCommon.h"
#include "Log.h"

#include "crypto/BLAKE3Hash.h"
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


void BinConsensusInstance::processMessage(const ptr<MessageEnvelope>& _me ) {

    CHECK_ARGUMENT( _me );
    auto msg = _me->getMessage();
    CHECK_STATE(msg);

    CHECK_STATE(msg->getBlockID() == getBlockID());
    CHECK_STATE(msg->getBlockProposerIndex() == getBlockProposerIndex());

    auto msgType = _me->getMessage()->getMessageType();

    auto msgOrigin = _me->getOrigin();

    if (msgOrigin == ORIGIN_NETWORK) {

        if (msgType != MSG_BVB_BROADCAST && msgType != MSG_AUX_BROADCAST)
            return;

        auto nwe = dynamic_pointer_cast<NetworkMessageEnvelope>( _me );
        CHECK_STATE(nwe);

        processNetworkMessageImpl(nwe);
        return;
    } else if (msgOrigin == ORIGIN_PARENT) {
        auto ime = dynamic_pointer_cast<InternalMessageEnvelope>( _me );
        CHECK_STATE(ime);
        processParentProposal(ime);
    }
}

void BinConsensusInstance::ifAlreadyDecidedSendDelayedEstimateForNextRound(bin_consensus_round _round) {
    if (isDecided && _round == getCurrentRound() + 1 && isTwoThird(totalAUXVotes(getCurrentRound()))) {
        LOG(debug,
            to_string(getBlockProposerIndex()) + ":NEW_ROUND_REQUESTED:BLOCK:" + to_string(blockID) + ":ROUND:" + to_string(getCurrentRound() + 1));
        proceedWithNextRound(decidedValue);
    }
}


void BinConsensusInstance::processNetworkMessageImpl(const ptr<NetworkMessageEnvelope>& _me) {
    CHECK_STATE(_me)
    updateStats(_me);

    auto message = dynamic_pointer_cast<NetworkMessage>(_me->getMessage());
    CHECK_STATE(message);
    CHECK_STATE(blockConsensusInstance);
    CHECK_STATE(blockConsensusInstance->fastMessageLedger);
    this->blockConsensusInstance->fastMessageLedger->writeNetworkMessage(message);

    auto round = message->getRound();
    addToHistory(message);

    CHECK_STATE2(round <= getCurrentRound() + 1, to_string(round) + ":" +
    to_string(getCurrentRound()) + ":" + to_string(getBlockID())) ;
    if (_me->getMessage()->getMessageType() == MSG_BVB_BROADCAST) {
        auto m = dynamic_pointer_cast<BVBroadcastMessage>(_me->getMessage());
        CHECK_STATE(m);
        if (! bvbVote(_me)) {
            // duplicate vote
            return;
        }
        networkBroadcastValueIfThird(m);
        ifAlreadyDecidedSendDelayedEstimateForNextRound(m->getRound());
        addToBinValuesIfTwoThirds(m);
    } else if (_me->getMessage()->getMessageType() == MSG_AUX_BROADCAST) {
        auto m = dynamic_pointer_cast<AUXBroadcastMessage>(_me->getMessage());
        CHECK_STATE(m);
        if (!auxVote(_me)) {
            // duplicate vote received
            return;
        }

        // Decision lottery has not yet been done for this round.
        if (m->getRound() == getCurrentRound())
            proceedWithDecisionLotteryIfAUXTwoThird(m->getRound());
    }
}

void BinConsensusInstance::updateStats(const ptr<NetworkMessageEnvelope> &_me) {

    CHECK_ARGUMENT(_me);

    auto processingTime = Time::getCurrentTimeMs() - _me->getArrivalTime();

    if (processingTime > maxProcessingTimeMs) {
        maxProcessingTimeMs = processingTime;
    }

    auto m = dynamic_pointer_cast<NetworkMessage>(_me->getMessage());
    CHECK_STATE(m);

    auto latencyTime = (int64_t) _me->getArrivalTime() - (int64_t) m->getTimeMs();
    if (latencyTime < 0)
        latencyTime = 0;

    if (maxLatencyTimeMs < (uint64_t) latencyTime) {
        maxLatencyTimeMs = (uint64_t) latencyTime;
    }
}

void BinConsensusInstance::processParentProposal(const ptr<InternalMessageEnvelope>& _me) {


    auto m = dynamic_pointer_cast<BVBroadcastMessage>(_me->getMessage());

    addToHistory(dynamic_pointer_cast<NetworkMessage>(m));

    CHECK_STATE(m->getRound() == 0);

    setProposal(m->getRound(), m->getValue());

    networkBroadcastValue(m);

    addBVSelfVoteToHistory(m->getRound(), m->getValue());

    bvbVote(_me);

    addToBinValuesIfTwoThirds(m);

}


void BinConsensusInstance::addToHistory(const ptr<NetworkMessage>&
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


[[nodiscard]] bool BinConsensusInstance::bvbVote(const ptr<MessageEnvelope>& _me) {

    auto m = dynamic_pointer_cast<BVBroadcastMessage>(_me->getMessage());
    CHECK_STATE(m);
    bin_consensus_round r = m->getRound();
    bin_consensus_value v = m->getValue();

    schain_index index = _me->getSrcSchainIndex();

    getSchain()->getNode()->getConsensusStateDB()->writeBVBVote(getBlockID(),
                                                                getBlockProposerIndex(), r, index, v);


    return bvbVoteCore(r, v, index);

}

bool BinConsensusInstance::bvbVoteCore(const bin_consensus_round &_r, const bin_consensus_value &_v,
                                       const schain_index &_index) {

    if (_v) {
        return bvbTrueVotes[_r].insert(_index).second;
    } else {
        return bvbFalseVotes[_r].insert(_index).second;
    }
}


[[nodiscard]] bool BinConsensusInstance::auxVote(const ptr<MessageEnvelope>& _me) {
    auto m = dynamic_pointer_cast<AUXBroadcastMessage>(_me->getMessage());
    auto r = m->getRound();
    bin_consensus_value v = m->getValue();

    auto index = _me->getSrcSchainIndex();


    ptr<ThresholdSigShare> sigShare = nullptr;

    if (r >= COMMON_COIN_ROUND) {
        sigShare = m->getSigShare();
    }

    return auxVoteCore(r, v, index, sigShare);
}

bool
BinConsensusInstance::auxVoteCore(const bin_consensus_round &_r, const bin_consensus_value &_v, const schain_index &_index,
                                  const ptr<ThresholdSigShare> &_sigShare) {

    if (_r >= COMMON_COIN_ROUND) {
        CHECK_STATE(_sigShare);
    } else {
        CHECK_STATE(_sigShare == nullptr);
    }

    if (_v) {
            return auxTrueVotes[_r].insert({_index, _sigShare}).second;
    } else {
            return auxFalseVotes[_r].insert({_index, _sigShare}).second;
    }
}


uint64_t BinConsensusInstance::totalAUXVotes(bin_consensus_round r) {
    return auxTrueVotes[r].size() + auxFalseVotes[r].size();
}

void BinConsensusInstance::auxSelfVote(bin_consensus_round _r,
        bin_consensus_value _v, const ptr<ThresholdSigShare>& _sigShare) {


    addAUXSelfVoteToHistory(_r, _v);

    auxVoteCore(_r,  _v, getSchain()->getSchainIndex(), _sigShare);

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

bool BinConsensusInstance::isThirdVote(const ptr<BVBroadcastMessage>& _m) {
    auto voteCount = getBVBVoteCount(_m->getValue(), _m->getRound());
    return isThird(voteCount);
}


bool BinConsensusInstance::isTwoThirdVote(const ptr<BVBroadcastMessage>& _m) {
    return isTwoThird(getBVBVoteCount(_m->getValue(), _m->getRound()));
}

void BinConsensusInstance::insertIntoBinValues(bin_consensus_round _r, bin_consensus_value _v) {
    getSchain()->getNode()->getConsensusStateDB()->writeBinValue(getBlockID(),
            getBlockProposerIndex(), _r, _v);
    binValues[_r].insert(_v);
}

void BinConsensusInstance::addToBinValuesIfTwoThirds(const ptr<BVBroadcastMessage>& _m) {

    auto r = _m->getRound();
    auto v = _m->getValue();



    if (binValues[r].count(v)) {
        // bin values already includes the value in question
        return;
    }

    if (isTwoThirdVote(_m)) {

        // Section 3.2 (06) the value is not yet in bin values, and 2t+1 nodes voted for it. Insert.
        insertIntoBinValues(r, v);

        // Section 4.2 (04) The first time binValues is updated it is broadcast
        // Also the aux message needs to be added as self vote in
        // Section 4.2 (05)
        bool didAUXBroadcast = binValues[r].size() > 1;
        if (!didAUXBroadcast) {
            auxSelfVoteAndBroadcastValue(r, v);
        }

        if (r == getCurrentRound()) {
            // Section 4.2 steps (07, 08, 09, 10) have not yet been performed for
            // r. Do them if received TwoThird of AUX messages
            // Note that condition (05) can be fullfiled
            // (1) if a message is added to binValues
            // (2) if a new SUX messages comes.
            // The below check corresponds to case (1)
            proceedWithDecisionLotteryIfAUXTwoThird(r);
        }

    }

}


void BinConsensusInstance::networkBroadcastValueIfThird(const ptr<BVBroadcastMessage>& _m) {
    if (isThirdVote(_m)) {
        // Section 3.2 (02)(03)(04) BVB value has been received from 2 t + 1 nodes. Broadcast it
        // if it has not yet been broadcast
        networkBroadcastValue(_m);
    }
}

void BinConsensusInstance::networkBroadcastValue(const ptr<BVBroadcastMessage>& _m) {

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


void BinConsensusInstance::auxSelfVoteAndBroadcastValue(bin_consensus_round _r, bin_consensus_value _v) {

    auto m = make_shared<AUXBroadcastMessage>(_r, _v, blockID, blockProposerIndex,
            Time::getCurrentTimeMs(), *this);

    if (_r >= COMMON_COIN_ROUND) {
        auxSelfVote( _r, _v, m->getSigShare() );
    } else {
        auxSelfVote(_r, _v, nullptr);
    }

    getSchain()->getNode()->getNetwork()->broadcastMessage(m);

}


void BinConsensusInstance::proceedWithDecisionLotteryIfAUXTwoThird(bin_consensus_round _r) {

    if (decided()) {
        // bin consensus has been decided. The only thing that
        // consensus will do after it has been decided is send the decided value in the next round.
        return;
    }

    CHECK_STATE(_r == getCurrentRound());

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
        // Section 4.2 (06)
        auto randomN = computeRandom(_r);


        // Section 4.2 Lines (07)(08)(09)(10)
        playDecisionLottery(hasTrue, hasFalse, randomN);
    }

}

uint64_t BinConsensusInstance::computeRandom(bin_consensus_round &_r) {
    uint64_t randomN;
    if (getSchain()->getNode()->isSgxEnabled() && ((uint64_t) _r) >= COMMON_COIN_ROUND) {
        randomN = calculateBLSRandom(_r);
    } else {

        string key = to_string((uint64_t ) getBlockID()) + ":" +
                     to_string((uint64_t)_r) + ":" +
                     to_string((uint64_t) getBlockProposerIndex());

        auto  d = make_shared<vector<uint8_t>>();

        for (uint64_t z = 0; z < key.length(); z++ ) {
            d->push_back(key.at(z));
        }
        auto hash = BLAKE3Hash::calculateHash(d);
        randomN = *((uint64_t*) hash.data());
    }

    auto randomDB = getSchain()->getNode()->getRandomDB();

    randomDB->writeRandom(getBlockID(), getBlockProposerIndex(),
                          _r, randomN);


    return randomN;
}


void BinConsensusInstance::playDecisionLottery(bool _hasTrue, bool _hasFalse, uint64_t _random) {


    CHECK_STATE(!isDecided);

    LOG(debug,
        to_string(getBlockProposerIndex()) + "ROUND_COMPLETE:BLOCK:" + to_string(blockID) + ":ROUND:" + to_string(getCurrentRound()));

    bin_consensus_value common_coin_value(_random % 2 == 0);

    addCommonCoinToHistory(getCurrentRound(), common_coin_value);


    if (_hasTrue && _hasFalse) {
        // Section (4.2) (07) Got both values proceed with the next round
        LOG(debug,
            to_string(getBlockProposerIndex()) + ":NEW ROUND:BLOCK:" + to_string(blockID) + ":ROUND:" +
                to_string(getCurrentRound() + 1));
        // Section 4.2 (10)
        proceedWithNextRound(common_coin_value);
        return;
    } else {

        // Section 4.2 (08) Check if we are lucky
        bin_consensus_value v(_hasTrue);
        if (v == common_coin_value) {
            // Lucky. Decide.
            LOG(debug,
                ":PROPOSER:" + to_string(getBlockProposerIndex()) + ":DECIDED VALUE" + to_string(v) + ":ROUND:" +
                    to_string(getCurrentRound()));
            decide(v);
        } else {
            // Unlucky. Next round.
            LOG(debug,
                to_string(getBlockProposerIndex()) +":NEW ROUND:BLOCK:" + to_string(v) + ":ROUND:" +
                   to_string(getCurrentRound()));
            proceedWithNextRound(v);
        }
    }

}


void BinConsensusInstance::proceedWithNextRound(bin_consensus_value _value) {

    CHECK_STATE(getCurrentRound() < 100);
    CHECK_STATE(isTwoThird(totalAUXVotes(getCurrentRound())));

    setCurrentRound(getCurrentRound() + 1);

    setProposal(getCurrentRound(), _value);

    addNextRoundToHistory(getCurrentRound(), _value);

    auto m = make_shared<BVBroadcastMessage>(getBlockID(), getBlockProposerIndex(),
                                             getCurrentRound(), _value,
                                             Time::getCurrentTimeMs(), *this);

    ptr<MessageEnvelope> me = make_shared<MessageEnvelope>(ORIGIN_NETWORK, m, getSchain()->getSchainIndex());

    networkBroadcastValue(m);

    addBVSelfVoteToHistory(m->getRound(), m->getValue());
    if (!bvbVote(me)) {
        // duplicate vote
        return;
    }

    addToBinValuesIfTwoThirds(m);

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


void BinConsensusInstance::logGlobalStats() {
    string stats = "Decided round stats:";
    for (uint64_t i = 0; i < globalDecidedRoundStats.size(); i++) {
        stats.append(":");
        stats.append(to_string(globalDecidedRoundStats.at(i)));
    }

    LOG(info, stats);
}

void BinConsensusInstance::addDecideToGlobalHistory(bin_consensus_round _r, bin_consensus_value _decidedValue) {

        lock_guard<recursive_mutex> lock(historyMutex);


        if (_r < globalDecidedRoundStats.size()) {
            globalDecidedRoundStats.at( ( uint64_t ) _r ) =
                globalDecidedRoundStats.at( ( uint64_t ) _r ) + 1;
        }


        auto trueCache = globalTrueDecisions->at((uint64_t)getBlockProposerIndex() - 1);
        auto falseCache = globalFalseDecisions->at((uint64_t)getBlockProposerIndex() - 1);
        auto child = getSchain()->getBlockConsensusInstance()->getChild(
                this->getProtocolKey());

        CHECK_STATE(child);
        CHECK_STATE(trueCache);
        CHECK_STATE(falseCache);

        if (_decidedValue) {

            if (auto result = falseCache->getIfExists(((uint64_t ) getBlockID()));
                 result.has_value()) {
                printHistory();
                any_cast<ptr<BinConsensusInstance>>(result)->printHistory();
                LOG(err, "Double decide 1");
            }
            trueCache->put((uint64_t) getBlockID(), child);
        } else {

            if (auto result = trueCache->getIfExists(((uint64_t ) getBlockID()));
                result.has_value()) {
                printHistory();
                any_cast<ptr<BinConsensusInstance>>(result)->printHistory();
                LOG(err, "Double decide 2");
            }
            falseCache->put((uint64_t) getBlockID(), child);
        }
}

void BinConsensusInstance::decide(bin_consensus_value _b) {

    CHECK_STATE(!isDecided);

    setDecidedRoundAndValue(getCurrentRound(), bin_consensus_value(_b));

    addDecideToGlobalHistory(getCurrentRound(), decidedValue);

    auto msg = make_shared<ChildBVDecidedMessage>((bool) _b, *this, this->getProtocolKey(),
                                                  this->getCurrentRound(), maxProcessingTimeMs,
                                                  maxLatencyTimeMs);


    LOG(debug,
        "Decided value: " + to_string(decidedValue) + " for blockid:" +
               to_string(getBlockID()) + " proposer:" +
               to_string(getBlockProposerIndex()));

    auto envelope = make_shared<InternalMessageEnvelope>(ORIGIN_CHILD, msg, *getSchain());

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

vector<uint64_t> BinConsensusInstance::globalDecidedRoundStats(16,0);




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
            CHECK_STATE(item.second);
            shares->addSigShare(item.second);
            if (shares->isEnough())
                break;
        }
    }

    if (binValues[_r].count(bin_consensus_value(false)) > 0 && auxFalseVotes[_r].size() > 0) {
        for (auto &&item: auxFalseVotes[_r]) {
            CHECK_STATE(item.second);
            shares->addSigShare(item.second);
            if (shares->isEnough())
                break;
        }
    }

    CHECK_STATE(shares->isEnough());

    auto random = shares->mergeSignature()->getRandom();

    LOG(debug,
        to_string(getBlockProposerIndex()) +
        ":Random for round: " + to_string(_r) + ":" + to_string(random));

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
