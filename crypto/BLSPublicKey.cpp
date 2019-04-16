//
// Created by kladko on 3/29/19.
//


#include "../SkaleConfig.h"
#include "../thirdparty/json.hpp"
#include "../Log.h"
#include "../network/Utils.h"

#include "../exceptions/InvalidArgumentException.h"

#include "bls_include.h"

#include "BLSPublicKey.h"

BLSPublicKey::BLSPublicKey(
    const string& k1, const string& k2, const string& k3, const string& k4, node_count _nodeCount )
    : nodeCount( static_cast< uint64_t >( _nodeCount ) ) {
    pk = make_shared< libff::alt_bn128_G2 >();

    pk->X.c0 = libff::alt_bn128_Fq( k1.c_str() );
    pk->X.c1 = libff::alt_bn128_Fq( k2.c_str() );
    pk->Y.c0 = libff::alt_bn128_Fq( k3.c_str() );
    pk->Y.c1 = libff::alt_bn128_Fq( k4.c_str() );
    pk->Z.c0 = libff::alt_bn128_Fq::one();
    pk->Z.c1 = libff::alt_bn128_Fq::zero();

    if ( pk->X.c0 == libff::alt_bn128_Fq::zero() || pk->X.c1 == libff::alt_bn128_Fq::zero() ||
         pk->Y.c0 == libff::alt_bn128_Fq::zero() || pk->Y.c1 == libff::alt_bn128_Fq::zero() ) {
        BOOST_THROW_EXCEPTION(
            InvalidArgumentException( "Public Key is equal to zero or corrupt", __CLASS_NAME__ ) );
    }



}
