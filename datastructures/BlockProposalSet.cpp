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

    @file BlockProposalSet.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../SkaleCommon.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"

#include "../node/ConsensusEngine.h"
#include "../crypto/SHAHash.h"
#include "../datastructures/BlockProposal.h"


#include "../chains/Schain.h"
#include "../pendingqueue/PendingTransactionsAgent.h"
#include "../blockproposal/received/ReceivedBlockProposalsDatabase.h"
#include "BlockProposal.h"

#include "BlockProposalSet.h"


using namespace std;




bool BlockProposalSet::addProposal(ptr<BlockProposal> _proposal) {
    ASSERT( _proposal );

    lock_guard< recursive_mutex > lock( proposalsMutex );

    if ( proposals.count(_proposal->getProposerIndex()) > 0 ) { // XXXX
        LOG(err,
            "Got block proposal with the same index" + to_string((uint64_t) _proposal->getProposerIndex()));
        return false;
    }

    proposals[_proposal->getProposerIndex()] = _proposal; // XXXX

    return true;

}


bool BlockProposalSet::isTwoThird() {
    lock_guard< recursive_mutex > lock( proposalsMutex );

    auto value = 3 * proposals.size() > 2 * sChain->getNodeCount();

    return value;
}


BlockProposalSet::BlockProposalSet( Schain* _sChain, block_id /*_blockID*/ )
    : sChain( _sChain ){
    totalObjects++;
}

BlockProposalSet::~BlockProposalSet() {
    totalObjects--;

}

node_count BlockProposalSet::getTotalProposalsCount() {
    lock_guard< recursive_mutex > lock( proposalsMutex );

    return ( node_count ) proposals.size();
}


ptr< vector< bool > > BlockProposalSet::createBooleanVector() {
    lock_guard< recursive_mutex > lock( proposalsMutex );

    auto v = make_shared<vector<bool>>( ( uint64_t ) sChain->getNodeCount() );

    for ( uint64_t i = 1; i <= sChain->getNodeCount(); i++ ) {
        ( *v )[i - 1] = ( proposals.count( schain_index( i )) > 0 ); // XXXX
    }

    return v;
};


ptr< BlockProposal > BlockProposalSet::getProposalByIndex( schain_index _index ) {
    lock_guard< recursive_mutex > lock( proposalsMutex );


    if ( proposals.count( _index) == 0 ) { // XXXX
        LOG(trace,
            "Proposal did not yet arrive. Total proposals:" + to_string(proposals.size()));
        return nullptr;
    }

    return proposals[_index]; // XXXX
}

atomic<uint64_t>  BlockProposalSet::totalObjects(0);