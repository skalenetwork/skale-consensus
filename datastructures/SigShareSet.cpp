/*
    Copyright (C) 2019 SKALE Labs

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

    @file SigShareSet.cpp
    @author Stan Kladko
    @date 2019
*/

#include "../SkaleCommon.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../crypto/bls_include.h"
#include "../node/ConsensusEngine.h"
#include "../crypto/SHAHash.h"
#include "../crypto/BLSSignature.h"

#include "../chains/Schain.h"
#include "../pendingqueue/PendingTransactionsAgent.h"
#include "../crypto/BLSSigShare.h"

#include "SigShareSet.h"


using namespace std;


bool SigShareSet::addSigShare( ptr< BLSSigShare > _sigShare ) {
    ASSERT( _sigShare );

    lock_guard< recursive_mutex > lock( sigSharesMutex );

    if ( sigShares.count( _sigShare->getSignerIndex()) > 0 ) { /
        LOG( err, "Got block proposal with the same index" +
                      to_string( ( uint64_t ) _sigShare->getSignerIndex()) ); // XXX
        return false;
    }

    sigShares[_sigShare->getSignerIndex()] = _sigShare; // XXX

    return true;
}


bool SigShareSet::isTwoThird() {

    lock_guard< recursive_mutex > lock( sigSharesMutex );


    auto nodeCount = sChain->getNodeCount();



    if ( nodeCount <= 2 ) {
        return nodeCount == sigShares.size();
    }
    return ( 3 * sigShares.size() > 2 * nodeCount );
}


bool SigShareSet::isTwoThirdMinusOne() {
    lock_guard< recursive_mutex > lock( sigSharesMutex );

    auto nodeCount = sChain->getNodeCount();

    if ( nodeCount <= 2 ) {
        return nodeCount - 1 == sigShares.size();
    }

    return ( 3 * ( sigShares.size() + 1 ) > 2 * nodeCount );
}


SigShareSet::SigShareSet( Schain* _sChain, block_id _blockId )
    : sChain( _sChain ), blockId( _blockId ) {
    totalObjects++;
}

SigShareSet::~SigShareSet() {
    totalObjects--;
}

node_count SigShareSet::getTotalSigSharesCount() {
    lock_guard< recursive_mutex > lock( sigSharesMutex );

    return ( node_count ) sigShares.size();
}


ptr< BLSSigShare > SigShareSet::getSigShareByIndex( schain_index _index ) {
    lock_guard< recursive_mutex > lock( sigSharesMutex );


    if ( sigShares.count( _index ) == 0 ) {
        LOG( trace,
            "Proposal did not yet arrive. Total sigShares:" + to_string( sigShares.size() ) );
        return nullptr;
    }

    return sigShares[_index];
}

atomic< uint64_t > SigShareSet::totalObjects( 0 );

ptr< BLSSignature > SigShareSet::mergeSignature() {
    signatures::Bls obj = signatures::Bls( 2, 2 );

    std::vector< size_t > participatingNodes;
    std::vector< libff::alt_bn128_G1 > shares;

    for ( auto&& item : sigShares ) {
        participatingNodes.push_back( static_cast< uint64_t >( item.first ) + 1 );
        shares.push_back( *item.second->getSig() );
    }

    /*
        libff::alt_bn128_G2 pk;

        // correct public key for secret keys from previous test
        pk.X.c0 =
       libff::alt_bn128_Fq("3587726236349347862079704257548861220640944168911165295818761560004029551650");
        pk.X.c1 =
       libff::alt_bn128_Fq("19787254980733313985916848161712839039049583927978588316450905648226551363679");
        pk.Y.c0 =
       libff::alt_bn128_Fq("6758417170296194890394379186698826295431221115224861568917420522501294769196");
        pk.Y.c1 =
       libff::alt_bn128_Fq("1055763161413596692895291379377477236343960686086193159772574402659834140867");
        pk.Z.c0 = libff::alt_bn128_Fq::one();
        pk.Z.c1 = libff::alt_bn128_Fq::zero();
    */


    std::vector<libff::alt_bn128_Fr> lagrangeCoeffs = obj.LagrangeCoeffs( participatingNodes );

    libff::alt_bn128_G1 signature = obj.SignatureRecover( shares, lagrangeCoeffs );

    auto sigPtr = make_shared<libff::alt_bn128_G1>( signature );

    // BOOST_REQUIRE(obj.Verification(hash, common_signature, pk) == false);

    return make_shared<BLSSignature>( sigPtr, blockId );
}
