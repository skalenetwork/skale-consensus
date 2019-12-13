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

    @file BlockConsensusAgent.h
    @author Stan Kladko
    @date 2018
*/

#pragma once

#include "protocols/ProtocolKey.h"
#include "protocols/ProtocolInstance.h"

class ChildBVDecidedMessage;
class BlockProposalSet;
class Schain;
class BooleanProposalVector;
class BlockSignBroadcastMessage;
class CryptoManager;


#include "thirdparty/lrucache.hpp"

class BlockConsensusAgent : public ProtocolInstance {

    class Comparator {
    public:
        bool operator()(const ptr<ProtocolKey> &a,
                        const ptr<ProtocolKey>& b ) const {

            return *a < *b;
        }
    };


    recursive_mutex childrenMutex;

    map<ptr<ProtocolKey>, ptr<BinConsensusInstance>, Comparator> children;

    ptr<cache::lru_cache<uint64_t , ptr<set<schain_index>>>> trueDecisions;
    ptr<cache::lru_cache<uint64_t , ptr<set<schain_index>>>> falseDecisions;
    ptr<cache::lru_cache<uint64_t , schain_index>> decidedIndices;


    void processChildMessageImpl(ptr<InternalMessageEnvelope> _me);

    void decideBlock(block_id _blockId, schain_index _sChainIndex);

    void propose(bin_consensus_value _proposal, schain_index index, block_id _id);

    void reportConsensusAndDecideIfNeeded(ptr<ChildBVDecidedMessage> _msg);

    void decideEmptyBlock(block_id _blockNumber);

    void startConsensusProposal(block_id _blockID, ptr<BooleanProposalVector> _proposal);

public:


    BlockConsensusAgent(Schain& _schain);


    bin_consensus_round getRound(ptr<ProtocolKey> _key);

    bool decided(ptr<ProtocolKey> key);





    void routeAndProcessMessage(ptr<MessageEnvelope> m);

    ptr<BinConsensusInstance> getChild(ptr<ProtocolKey> key);


    void processBlockSignMessage(ptr<BlockSignBroadcastMessage> _message);
};

