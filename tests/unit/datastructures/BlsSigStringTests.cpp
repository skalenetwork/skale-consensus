/*
    Copyright (C) 2026 SKALE Labs

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

    @file BlsSigStringTests.cpp
    @date 2026
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "crypto/bls_include.h"
#include "crypto/BLAKE3Hash.h"
#include "crypto/ConsensusBLSSigShare.h"
#include "crypto/ConsensusBLSSignature.h"
#include "crypto/ConsensusSigShareSet.h"
#include "chains/Schain.h"

#include "libBLS/bls/BLSPrivateKeyShare.h"
#include "libBLS/bls/BLSPublicKey.h"
#include "libBLS/bls/BLSSigShare.h"
#include "libBLS/bls/BLSSignature.h"

#include "libBLS/tools/utils.h"

#include <mutex>

#include "thirdparty/catch.hpp"

namespace {

const size_t REQUIRED_SIGNERS = 3;
const size_t TOTAL_SIGNERS = 4;
const uint64_t PROPOSER_INDEX = 1;
const uint64_t BLOCK_ID = 7;
const uint64_t SCHAIN_ID = 1;

vector< string > splitSigString( const string& _s ) {
    vector< string > tokens;
    size_t prev = 0;
    while ( true ) {
        size_t pos = _s.find( ':', prev );
        if ( pos == string::npos ) {
            tokens.push_back( _s.substr( prev ) );
            break;
        }
        tokens.push_back( _s.substr( prev, pos - prev ) );
        prev = pos + 1;
    }
    return tokens;
}

// merges the given shares with the consensus wrappers and returns the resulting signature
ptr< ConsensusBLSSignature > mergeShares( const vector< ptr< libBLS::BLSSigShare > >& _shares ) {
    ConsensusSigShareSet set( block_id( BLOCK_ID ), TOTAL_SIGNERS, REQUIRED_SIGNERS );
    for ( auto& share : _shares ) {
        auto wrapped =
            make_shared< ConsensusBLSSigShare >( share, schain_id( SCHAIN_ID ), block_id( BLOCK_ID ) );
        CATCH_REQUIRE( set.addSigShare( wrapped ) );
    }
    CATCH_REQUIRE( set.isEnough() );
    auto sig = dynamic_pointer_cast< ConsensusBLSSignature >( set.mergeSignature() );
    CATCH_REQUIRE( sig );
    return sig;
}

}  // namespace

CATCH_TEST_CASE( "Merged BLS signature string matches the canonical recomputation",
    "[bls-sig-string][unit][correctness]" ) {
    // libBLS must be initialized in all build flavors
    static std::once_flag libBLSInitFlag;
    std::call_once( libBLSInitFlag, []() { libBLS::init(); } );

    auto keys = libBLS::BLSPrivateKeyShare::generateSampleKeys( REQUIRED_SIGNERS, TOTAL_SIGNERS );
    auto& privateKeys = keys.first;

    auto hash = BLAKE3Hash::getConsensusHash( PROPOSER_INDEX, BLOCK_ID, SCHAIN_ID );

    // signer indices are 1-based
    vector< ptr< libBLS::BLSSigShare > > shares;
    for ( size_t i = 1; i <= TOTAL_SIGNERS; i++ ) {
        shares.push_back( make_shared< libBLS::BLSSigShare >(
            privateKeys.at( i - 1 ).sign( hash.getHash(), i ) ) );
    }

    auto sigA = mergeShares( { shares.at( 0 ), shares.at( 1 ), shares.at( 2 ) } );
    auto sigB = mergeShares( { shares.at( 1 ), shares.at( 2 ), shares.at( 3 ) } );

    string stringA = sigA->toString();
    string stringB = sigB->toString();

    CATCH_SECTION( "Different share sets give identical strings" ) {
        CATCH_REQUIRE( stringA == stringB );
        CATCH_REQUIRE( Schain::calculateRandomFromSignatureString( stringA ) ==
                       Schain::calculateRandomFromSignatureString( stringB ) );
    }

    CATCH_SECTION( "Hint is the canonical recomputation over the signed hash" ) {
        auto hashWithHint = libBLS::algebra::hashToG1withHint( hash.getHash() );
        string expectedHint =
            hashWithHint.first.getY().toString( libBLS::Base::DEC ) + ":" + hashWithHint.second;
        CATCH_REQUIRE( sigA->getBlsSig()->getHint() == expectedHint );
        CATCH_REQUIRE( sigB->getBlsSig()->getHint() == expectedHint );

        auto tokens = splitSigString( stringA );
        CATCH_REQUIRE( tokens.size() == 4 );
        CATCH_REQUIRE( tokens.at( 2 ) + ":" + tokens.at( 3 ) == expectedHint );
    }
}
