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
#include "FastMessageLedger.h"

class BlockConsensusAgent : public ProtocolInstance {
    friend class BinConsensusInstance;

    recursive_mutex m;

    // protocol cache for each block proposer

    vector< ptr< cache::lru_cache< uint64_t, ptr< BinConsensusInstance > > > > children;  // tsafe

    ptr< cache::lru_cache< uint64_t, ptr< map< schain_index, ptr< ChildBVDecidedMessage > > > > >
        trueDecisions;
    ptr< cache::lru_cache< uint64_t, ptr< map< schain_index, ptr< ChildBVDecidedMessage > > > > >
        falseDecisions;
    ptr< cache::lru_cache< uint64_t, schain_index > > decidedIndices;

    void processChildMessageImpl( const ptr< InternalMessageEnvelope >& _me );

    void decideBlock( block_id _blockId, schain_index _sChainIndex, const string& _stats );

    void propose( bin_consensus_value _proposal, schain_index index, block_id _id );

    void reportConsensusAndDecideIfNeeded( const ptr< ChildBVDecidedMessage >& _msg );

    void decideDefaultBlock( block_id _blockNumber );


    void startConsensusProposal( block_id _blockID, const ptr< BooleanProposalVector >& _proposal );

    void processBlockSignMessage( const ptr< BlockSignBroadcastMessage >& _message );


    bin_consensus_round getRound( const ptr< ProtocolKey >& _key );


    bool decided( const ptr< ProtocolKey >& _key );

    string buildStats( block_id _blockID );

    ptr< BinConsensusInstance > getChild( const ptr< ProtocolKey >& _key );

    void writeString( string& _str );

public:
    BlockConsensusAgent( Schain& _schain );

    bool shouldPost( const ptr< NetworkMessage >& _msg );

    void routeAndProcessMessage( const ptr< MessageEnvelope >& _me );
};
