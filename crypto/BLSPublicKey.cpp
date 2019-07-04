//
// Created by kladko on 7/4/19.
//


#include <stdint.h>
#include <string>

using namespace std;

#include "../crypto/bls_include.h"
#include "BLSPublicKey.h"

BLSPublicKey::BLSPublicKey( const string& k1, const string& k2, const string& k3, const string& k4,
    size_t _totalSigners, size_t _requiredSigners )
    : requiredSigners( _requiredSigners ), totalSigners( _totalSigners ) {
    pk = make_shared< libff::alt_bn128_G2 >();

    pk->X.c0 = libff::alt_bn128_Fq( k1.c_str() );
    pk->X.c1 = libff::alt_bn128_Fq( k2.c_str() );
    pk->Y.c0 = libff::alt_bn128_Fq( k3.c_str() );
    pk->Y.c1 = libff::alt_bn128_Fq( k4.c_str() );
    pk->Z.c0 = libff::alt_bn128_Fq::one();
    pk->Z.c1 = libff::alt_bn128_Fq::zero();

    if ( pk->X.c0 == libff::alt_bn128_Fq::zero() || pk->X.c1 == libff::alt_bn128_Fq::zero() ||
         pk->Y.c0 == libff::alt_bn128_Fq::zero() || pk->Y.c1 == libff::alt_bn128_Fq::zero() ) {
        BOOST_THROW_EXCEPTION( runtime_error( "Public Key is equal to zero or corrupt" ) );
    }
}

