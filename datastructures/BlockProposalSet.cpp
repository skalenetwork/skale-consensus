#include "../SkaleConfig.h"
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

    if ( proposals.count(_proposal->getProposerIndex()) > 0 ) {
        LOG(err,
            "Got block proposal with the same index" + to_string((uint64_t) _proposal->getProposerIndex()));
        return false;
    }

    proposals[_proposal->getProposerIndex()] = _proposal;

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

    auto v = make_shared< vector< bool > >( ( uint64_t ) sChain->getNodeCount() );

    for ( uint64_t i = 0; i < sChain->getNodeCount(); i++ ) {
        ( *v )[i] = ( proposals.count( schain_index( i ) ) > 0 );
    }

    return v;
};


ptr< BlockProposal > BlockProposalSet::getProposalByIndex( schain_index _index ) {
    lock_guard< recursive_mutex > lock( proposalsMutex );





    if ( proposals.count( _index ) == 0 ) {
        LOG(trace,
            "Proposal did not yet arrive. Total proposals:" + to_string(proposals.size()));
        return nullptr;
    }

    return proposals[_index];
}

atomic<uint64_t>  BlockProposalSet::totalObjects(0);