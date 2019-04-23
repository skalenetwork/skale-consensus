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

    @file BLSPrivateKey.cpp
    @author Stan Kladko
    @date 2019
*/


#include "../Log.h"
#include "../SkaleConfig.h"
#include "../exceptions/InvalidArgumentException.h"
#include "../network/Utils.h"
#include "../thirdparty/json.hpp"
#include "SHAHash.h"

#pragma GCC diagnostic push
// Suppress warnings: "unknown option after ‘#pragma GCC diagnostic’ kind [-Wpragmas]".
// This is necessary because not all the compilers have the same warning options.
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wdeprecated-register"
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wmismatched-tags"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wtautological-compare"
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunneeded-internal-declaration"
#pragma GCC diagnostic ignored "-Wunused-private-field"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wignored-attributes"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#pragma GCC diagnostic ignored "-Wchar-subscripts"
#pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Wreorder"
#include "bls.h"
#pragma GCC diagnostic pop

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <iostream>


#include "BLSPrivateKey.h"
#include "BLSSigShare.h"


BLSPrivateKey::BLSPrivateKey( const string& k, node_count _nodeCount )
    : nodeCount( static_cast< uint64_t >( _nodeCount ) ) {
    sk = make_shared< libff::alt_bn128_Fr >( k.c_str() );

    if ( *sk == libff::alt_bn128_Fr::zero() ) {
        BOOST_THROW_EXCEPTION( InvalidArgumentException(
            "Secret key share is equal to zero or corrupt", __CLASS_NAME__ ) );
    }
}


ptr< BLSSigShare > BLSPrivateKey::sign(
    ptr< string > _msg, block_id _blockId, schain_index _signerIndex, node_id _signerNodeId ) {
    ptr< signatures::Bls > obj;

    if ( nodeCount == 1 || nodeCount == 2 ) {
        obj = make_shared< signatures::Bls >( signatures::Bls( nodeCount, nodeCount ) );  // test
    } else {
        obj = make_shared< signatures::Bls >( signatures::Bls( 2 * ( nodeCount / 3 ), nodeCount ) );
    }

    libff::alt_bn128_G1 hash = obj->Hashing( *_msg );

    auto ss = make_shared< libff::alt_bn128_G1 >( obj->Signing( hash, *sk ) );

    ss->to_affine_coordinates();

    auto s = make_shared< BLSSigShare >( ss, _blockId, _signerIndex, _signerNodeId );

    auto ts = s->toString();

    auto sig2 = make_shared< BLSSigShare >( ts, _blockId, _signerIndex, _signerNodeId );

    assert( *s->getSig() == *sig2->getSig() );


    return s;
}





