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

    @file BlockConsensusAgent.h
    @author Stan Kladko
    @date 2018
*/

#pragma once



#include "../ProtocolKey.h"

#include "../ProtocolInstance.h"



class ChildBVDecidedMessage;
class BlockProposalSet;
class Schain;
class BooleanProposalVector;


class BlockConsensusAgent {

    Schain &  sChain;


    class Comparator {
    public:
        bool operator()(const ptr<ProtocolKey> &a,
                        const ptr<ProtocolKey>& b ) const {

            return *a < *b;
            return *a < *b;

        }
    };



    bin_consensus_round cleanedRound = bin_consensus_round(0);


    recursive_mutex childrenMutex;

    map<ptr<ProtocolKey>, ptr<BinConsensusInstance>, Comparator> children;




    map<ptr<ProtocolKey>, ProtocolOutcome , Comparator> completedInstancesByProtocolKey;


    set<block_id> proposedBlocks;

    map<block_id , set<schain_index>> trueDecisions;;

    map<block_id , set<schain_index>> falseDecisions;


    map<block_id, schain_index>  decidedBlocks;

    void processChildMessageImpl(ptr<InternalMessageEnvelope> _me);

    void decideBlock(block_id _blockId, schain_index subChainIndex);


    void propose(bin_consensus_value _proposal, schain_index index, block_id _id);

    void reportConsensusAndDecideIfNeeded(ptr<ChildBVDecidedMessage> msg);

    void voteAndDecideIfNeded1(ptr<ChildBVDecidedMessage> msg);

    void decideEmptyBlock(block_id blockNumber);

    void disconnect(ptr<ProtocolKey> key );

    void processChildCompletedMessage(ptr<InternalMessageEnvelope> _me);

    void startConsensusProposal(block_id _blockID, ptr<BooleanProposalVector> _proposal);


    void processMessage(ptr<MessageEnvelope> _m);


public:


    BlockConsensusAgent(Schain& _schain);


    bin_consensus_round getRound(ptr<ProtocolKey> _key);

    bool decided(ptr<ProtocolKey> key);


    Schain *getSchain() const;


    void routeAndProcessMessage(ptr<MessageEnvelope> m);

    ptr<BinConsensusInstance> getChild(ptr<ProtocolKey> key);


};

