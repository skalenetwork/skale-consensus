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

    @file BlockProposalSet.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"

#include "node/ConsensusEngine.h"
#include "crypto/BLAKE3Hash.h"
#include "datastructures/BlockProposal.h"
#include "datastructures/BooleanProposalVector.h"

#include "chains/Schain.h"
#include "pendingqueue/PendingTransactionsAgent.h"
#include "db/BlockProposalDB.h"
#include "datastructures/DAProof.h"


#include "BlockProposalSet.h"

using namespace std;

bool BlockProposalSet::add( const ptr< BlockProposal >& _proposal ) {
    CHECK_ARGUMENT( _proposal );

    auto index = ( uint64_t ) _proposal->getProposerIndex();

    CHECK_STATE( index > 0 && index <= nodeCount )

    LOCK( m )

    if ( proposals.count( index ) > 0 ) {
        LOG( trace, "Got block proposal with the same index" << to_string( index ) );
        return false;
    }

    proposals.emplace( index, _proposal );

    return true;
}


BlockProposalSet::BlockProposalSet( Schain* _sChain, block_id _blockId ) : blockId( _blockId ) {
    CHECK_ARGUMENT( _sChain );
    CHECK_ARGUMENT( _blockId > 0 );

    nodeCount = _sChain->getNodeCount();
    totalObjects++;
}

BlockProposalSet::~BlockProposalSet() {
    totalObjects--;
}

node_count BlockProposalSet::getCount() {
    LOCK( m )
    return ( node_count ) proposals.size();
}


atomic< int64_t > BlockProposalSet::totalObjects( 0 );
