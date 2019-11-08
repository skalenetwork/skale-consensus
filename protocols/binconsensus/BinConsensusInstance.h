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

    @file BinConsensusInstance.h
    @author Stan Kladko
    @date 2018
*/

#pragma  once


#include "../../protocols/ProtocolInstance.h"


static const int MSG_HISTORY_SIZE = 2048;

class BlockConsensusAgent;
class ConsensusBLSSigShare;

class BVBroadcastMessage;
class NetworkMessageEnvelope;
class Schain;
class ProtocolKey;



class BinConsensusInstance : public ProtocolInstance{


    BlockConsensusAgent * blockConsensusInstance;

    block_id blockID;

    schain_index blockProposerIndex;

    node_count nodeCount;
public:
    const node_count &getNodeCount() const;

private:


    bool isDecided = false;


    class Comparator {
    public:
        bool operator()(const ptr<ProtocolKey> &a,
                        const ptr<ProtocolKey>& b ) const {

            return *a < *b;

        }
    };


    map<ptr<ProtocolKey>, ptr<BinConsensusInstance>, Comparator> children;


    static recursive_mutex historyMutex;

    static ptr<map<ptr<ProtocolKey>, ptr<BinConsensusInstance>, Comparator>> globalTrueDecisions;

    static ptr<map<ptr<ProtocolKey>, ptr<BinConsensusInstance>, Comparator>> globalFalseDecisions;

    static ptr<list<ptr<NetworkMessage>>> msgHistory;

    bin_consensus_value decidedValue;

    bin_consensus_round decidedRound;


    const ptr<ProtocolKey> protocolKey;

    std::atomic<bin_consensus_round> currentRound = bin_consensus_round(0);

    bin_consensus_round commonCoinNextRound = bin_consensus_round(0);

    map  <bin_consensus_round, bin_consensus_value> est;

    map  <bin_consensus_round, bin_consensus_value>  w;





    map<bin_consensus_round, set<schain_index>> bvbTrueVotes;
    map<bin_consensus_round, set<schain_index>> bvbFalseVotes;

    map<bin_consensus_round, map<schain_index, ptr<ConsensusBLSSigShare>>> auxTrueVotes;
    map<bin_consensus_round, map<schain_index, ptr<ConsensusBLSSigShare>>> auxFalseVotes;



    map<bin_consensus_round, set<bin_consensus_value>> binValues;

    map<bin_consensus_round, set<bin_consensus_value>> broadcastValues;


    void processNetworkMessageImpl(ptr<NetworkMessageEnvelope> me);



    void networkBroadcastValueIfThird(ptr<BVBroadcastMessage>  m);

    void networkBroadcastValue(ptr<BVBroadcastMessage> m);


    void commitValueIfTwoThirds(ptr<BVBroadcastMessage> m);

    void bvbVote(ptr<MessageEnvelope> me);

    void auxVote(ptr<MessageEnvelope> ptr);


    node_count getBVBVoteCount(bin_consensus_value v, bin_consensus_round round);

    node_count getAUXVoteCount(bin_consensus_value v, bin_consensus_round round);

    bool isThirdVote(ptr<BVBroadcastMessage> m);


    void proceedWithCommonCoinIfAUXTwoThird(bin_consensus_round _r);

    void auxBroadcastValue(bin_consensus_value v, bin_consensus_round round);

    bool isThird(node_count count);

    bool isTwoThird(node_count count);

    void proceedWithCommonCoin(bool _hasTrue, bool _hasFalse, uint64_t _random);

    void proceedWithNewRound(bin_consensus_value value);

    void printHistory();

    void decide(bin_consensus_value b);

    bool isTwoThirdVote(ptr<BVBroadcastMessage> m);

    void ifAlreadyDecidedSendDelayedEstimateForNextRound(bin_consensus_round round);


    void initiateProtocolCompletion(ProtocolOutcome outcome);


    void processParentCompletedMessage(ptr<InternalMessageEnvelope> me);

    uint64_t totalAUXVotes(bin_consensus_round r);


    void auxSelfVote(bin_consensus_round r, bin_consensus_value v, ptr<ConsensusBLSSigShare> _sigShare);



public:



    bool decided() const;


    const block_id getBlockID() const;

    const schain_index getBlockProposerIndex() const;


    ptr<ProtocolKey> getProtocolKey() {
        ASSERT(protocolKey);
        return protocolKey;
    }



    void processMessage(ptr<MessageEnvelope> m);

    void processParentProposal(ptr<InternalMessageEnvelope> me);

    BinConsensusInstance(BlockConsensusAgent* instance, block_id _blockId, schain_index _blockProposerIndex);

    bin_consensus_round getCurrentRound();

    void addToHistory(shared_ptr<NetworkMessage> m);


    void addBVSelfVoteToHistory(bin_consensus_round _r, bin_consensus_value _v);

    void addAUXSelfVoteToHistory(bin_consensus_round _r, bin_consensus_value _v);

    void addCommonCoinToHistory(bin_consensus_round _r, bin_consensus_value _v);

    void addDecideToHistory(bin_consensus_round _r, bin_consensus_value _v);

    void addNextRoundToHistory(bin_consensus_round _r, bin_consensus_value _v);




    static void initHistory();

    BlockConsensusAgent *getBlockConsensusInstance() const;

    uint64_t calculateBLSRandom(bin_consensus_round _r);

};


