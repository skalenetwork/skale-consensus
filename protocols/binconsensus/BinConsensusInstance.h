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
    @date 2018=
*/

#pragma once


#include "protocols/ProtocolInstance.h"


static const int MSG_HISTORY_SIZE = 2048;

class BlockConsensusAgent;
class ConsensusBLSSigShare;

class ThresholdSigShare;
class BVBroadcastMessage;
class NetworkMessageEnvelope;
class Schain;
class ProtocolKey;

#include "thirdparty/lrucache.hpp"

class BinConsensusInstance : public ProtocolInstance {
    friend class BlockConsensusAgent;
    friend class HistoryMessage;

    BlockConsensusAgent* const blockConsensusInstance = nullptr;
    const block_id blockID = 0;
    ;
    const schain_index blockProposerIndex = 0;
    const node_count nodeCount = 0;
    const ptr< ProtocolKey > protocolKey;

    uint64_t maxProcessingTimeMs = 0;
    uint64_t maxLatencyTimeMs = 0;


    class Comparator {
    public:
        bool operator()( const ptr< ProtocolKey >& a, const ptr< ProtocolKey >& b ) const {
            return *a < *b;
        }
    };


    // non-essential debugging
    static recursive_mutex historyMutex;

    // non-essential debugging
    static ptr< vector< ptr< cache::lru_cache< uint64_t, ptr< BinConsensusInstance > > > > >
        globalTrueDecisions;

    // non-essential debugging
    static ptr< vector< ptr< cache::lru_cache< uint64_t, ptr< BinConsensusInstance > > > > >
        globalFalseDecisions;


    // non-essential globalRoundStats
    static vector< uint64_t > globalDecidedRoundStats;


    // non-essential tracing data tracing proposals for each round
    map< bin_consensus_round, bin_consensus_value > proposals;

    // Used to make sure the same message is not broadcast twice. Does not need to be
    // saved in the DB
    map< bin_consensus_round, set< bin_consensus_value > > broadcastValues;

#ifdef CONSENSUS_DEBUG

    // non-essential debugging
    static ptr< list< ptr< NetworkMessage > > > msgHistory;


#endif

    // THIS FIELDS are requred by the protocol and in general are persisted in LevelDB


    bool isDecided = false;  // does not have to be persisted in database since it is
    // enough to persist decidedValue and  decided round
    bin_consensus_value decidedValue;
    bin_consensus_round decidedRound;

    std::atomic< bin_consensus_round > currentRound = bin_consensus_round( 0 );

    map< bin_consensus_round, set< schain_index > > bvbTrueVotes;
    map< bin_consensus_round, set< schain_index > > bvbFalseVotes;

    map< bin_consensus_round, map< schain_index, ptr< ThresholdSigShare > > > auxTrueVotes;
    map< bin_consensus_round, map< schain_index, ptr< ThresholdSigShare > > > auxFalseVotes;

    map< bin_consensus_round, set< bin_consensus_value > > binValues;

    // END OF ESSENTIAL PROTOCOL FIELDS

    void processNetworkMessageImpl( const ptr< NetworkMessageEnvelope >& _me );


    void networkBroadcastValueIfThird( const ptr< BVBroadcastMessage >& _m );

    void networkBroadcastValue( const ptr< BVBroadcastMessage >& _m );

    void setProposal( bin_consensus_round _r, bin_consensus_value _v );


    void insertIntoBinValues( bin_consensus_round _r, bin_consensus_value _v );

    void addToBinValuesIfTwoThirds( const ptr< BVBroadcastMessage >& _m );

    bool bvbVote( const ptr< MessageEnvelope >& _me );

    bool auxVote( const ptr< MessageEnvelope >& _me );


    node_count getBVBVoteCount( bin_consensus_value _v, bin_consensus_round _round );

    node_count getAUXVoteCount( bin_consensus_value _v, bin_consensus_round _round );

    bool isThirdVote( const ptr< BVBroadcastMessage >& _m );


    void proceedWithDecisionLotteryIfAUXTwoThird( bin_consensus_round _r );

    void auxSelfVoteAndBroadcastValue( bin_consensus_round _r, bin_consensus_value _v );

    bool isThird( node_count _count );

    bool isTwoThird( node_count _count );

    void playDecisionLottery( bool _hasTrue, bool _hasFalse, uint64_t _random );

    void proceedWithNextRound( bin_consensus_value _value );

    void printHistory();

    void decide( bin_consensus_value _b );

    bool isTwoThirdVote( const ptr< BVBroadcastMessage >& _m );

    void ifAlreadyDecidedSendDelayedEstimateForNextRound( bin_consensus_round _round );


    uint64_t totalAUXVotes( bin_consensus_round _r );


    void auxSelfVote(
        bin_consensus_round _r, bin_consensus_value _v, const ptr< ThresholdSigShare >& _sigShare );

    void addToHistory( const ptr< NetworkMessage >& _m );

    void addBVSelfVoteToHistory( bin_consensus_round _r, bin_consensus_value _v );

    void addAUXSelfVoteToHistory( bin_consensus_round _r, bin_consensus_value _v );

    void addCommonCoinToHistory( bin_consensus_round _r, bin_consensus_value _v );

    void addDecideToHistory( bin_consensus_round _r, bin_consensus_value _v );

    void addNextRoundToHistory( bin_consensus_round _r, bin_consensus_value _v );

    uint64_t calculateBLSRandom( bin_consensus_round _r );

    void addDecideToGlobalHistory( bin_consensus_round _r, bin_consensus_value _decidedValue );

    void setCurrentRound( bin_consensus_round _currentRound );

    void setDecidedRoundAndValue(
        const bin_consensus_round& _decidedRound, const bin_consensus_value& _decidedValue );

    const node_count& getNodeCount() const;


    void updateStats( const ptr< NetworkMessageEnvelope >& _me );


    void processParentProposal( const ptr< InternalMessageEnvelope >& _me );

    void processMessage( const ptr< MessageEnvelope >& _me );


    bin_consensus_round getCurrentRound();

    void initFromDB( const BlockConsensusAgent* _instance );

    static void logGlobalStats();

    bool decided() const;

    const block_id getBlockID() const;

    const schain_index getBlockProposerIndex() const;

    ptr< ProtocolKey > getProtocolKey() {
        CHECK_STATE( protocolKey );
        return protocolKey;
    }


    bool bvbVoteCore(
        const bin_consensus_round& r, const bin_consensus_value& v, const schain_index& index );

    bool auxVoteCore( const bin_consensus_round& r, const bin_consensus_value& v,
        const schain_index& index, const ptr< ThresholdSigShare >& sigShare );

    uint64_t computeRandom( bin_consensus_round& _r );


public:
    BinConsensusInstance( BlockConsensusAgent* _instance, block_id _blockId,
        schain_index _blockProposerIndex, bool _initFromDB = false );


    static void initHistory( node_count _nodeCount );
};
