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
#include "../exceptions/FatalError.h"
#include "../exceptions/InvalidArgumentException.h"
#include "../network/Utils.h"
#include "../thirdparty/json.hpp"
#include "SHAHash.h"

#include "../crypto/bls_include.h"

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


ptr<BLSSigShare>
BLSPrivateKey::sign(ptr<string> _msg, schain_id _schainId, block_id _blockId, schain_index _signerIndex,
                    node_id _signerNodeId) {
    ptr< signatures::Bls > obj;

    if ( nodeCount == 1 || nodeCount == 2 ) {
        obj = make_shared< signatures::Bls >( signatures::Bls( nodeCount, nodeCount ) );  // test
    } else {
        obj = make_shared< signatures::Bls >( signatures::Bls( 2 * ( nodeCount / 3 ), nodeCount ) );
    }

    libff::alt_bn128_G1 hash = obj->Hashing( *_msg );

    auto ss = make_shared< libff::alt_bn128_G1 >( obj->Signing( hash, *sk ) );

    ss->to_affine_coordinates();

    auto s = make_shared< BLSSigShare >( ss, _schainId,  _blockId, _signerIndex, _signerNodeId );

    auto ts = s->toString();

    auto sig2 = make_shared< BLSSigShare >( ts, _schainId, _blockId, _signerIndex, _signerNodeId );

    ASSERT( *s->getSig() == *sig2->getSig() );


    return s;
}





