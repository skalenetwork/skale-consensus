/*
    Copyright (C) 2020-present SKALE Labs

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

    @file OpenSSLECDSAPrivateKey.cpp
    @author Stan Kladko
    @date 2020
*/

#include <openssl/ec.h>  // for EC_GROUP_new_by_curve_name, EC_GROUP_free, EC_KEY_new, EC_KEY_set_group, EC_KEY_generate_key, EC_KEY_free
#include <openssl/ecdsa.h>    // for ECDSA_do_sign, ECDSA_do_verify
#include <openssl/obj_mac.h>  // for NID_secp256k1


#include "openssl/bn.h"
#include "openssl/ecdsa.h"
#include "openssl/err.h"
#include "openssl/sha.h"
#include "openssl/x509.h"


#include "Log.h"

#include "OpenSSLECDSAKey.h"
#include "network/Utils.h"

#include "SkaleCommon.h"

OpenSSLECDSAKey::OpenSSLECDSAKey( EC_KEY* _ecKey, bool _isPrivate ) : isPrivate( _isPrivate ) {
    CHECK_STATE( _ecKey );
    this->ecKey = _ecKey;
}
OpenSSLECDSAKey::~OpenSSLECDSAKey() {
    if ( ecKey )
        EC_KEY_free( ecKey );
}
ptr< OpenSSLECDSAKey > OpenSSLECDSAKey::generateKey() {
    EC_KEY* eckey = nullptr;

    try {
        eckey = EC_KEY_new_by_curve_name( NID_secp256k1 );
        CHECK_STATE( eckey );
        if ( ecgroup == nullptr ) {
            ecgroup = EC_GROUP_new_by_curve_name( NID_secp256k1 );
            CHECK_STATE( ecgroup );
        }
        CHECK_STATE( EC_KEY_set_group( eckey, ecgroup ) == 1 );
        CHECK_STATE( EC_KEY_generate_key( eckey ) == 1 )
        CHECK_STATE( eckey );
    } catch ( ... ) {
        if ( eckey ) {
            EC_KEY_free( eckey );
        }
        throw;
    }

    return make_shared< OpenSSLECDSAKey >( eckey, true );
}

EC_GROUP* OpenSSLECDSAKey::ecgroup = nullptr;

ptr< string > OpenSSLECDSAKey::getPublicKey() {
    auto pubKeyComponent = EC_KEY_get0_public_key( ecKey );

    CHECK_STATE( pubKeyComponent );

    if ( ecgroup == nullptr ) {
        ecgroup = EC_GROUP_new_by_curve_name( NID_secp256k1 );
        CHECK_STATE( ecgroup );
    }

    char* hex = nullptr;
    ptr< string > result = nullptr;

    try {
        hex = EC_POINT_point2hex( ecgroup, pubKeyComponent, POINT_CONVERSION_COMPRESSED, NULL );

        CHECK_STATE( hex );

        result = make_shared< string >( hex );

    } catch ( ... ) {
        if ( hex )
            OPENSSL_free( hex );
        throw;
    }

    if ( hex )
        OPENSSL_free( hex );

    return result;
}

bool OpenSSLECDSAKey::verifySGXSig( ptr< string > _sig, const char* _hash ) {
    bool returnValue = false;

    try {
        auto firstColumn = _sig->find( ":" );

        if ( firstColumn == string::npos || firstColumn == _sig->length() - 1 ) {
            LOG( err, "Misfomatted signature" );
            goto clean;
        }

        auto secondColumn = _sig->find( ":", firstColumn + 1 );

        if ( secondColumn == string::npos || secondColumn == _sig->length() - 1 ) {
            LOG( err, "Misformatted signature" );
            goto clean;
        }

        auto r = _sig->substr( firstColumn + 1, secondColumn - firstColumn - 1 );
        auto s = _sig->substr( secondColumn + 1, _sig->length() - secondColumn - 1 );

        if ( r == s ) {
            LOG( err, "r == s " );
            goto clean;
        }

        CHECK_STATE( firstColumn != secondColumn );

        BIGNUM* rBN = BN_new();
        BIGNUM* sBN = BN_new();

        CHECK_STATE( BN_hex2bn( &rBN, r.c_str() ) != 0 );
        CHECK_STATE( BN_hex2bn( &sBN, s.c_str() ) != 0 );


        auto oSig = ECDSA_SIG_new();

        CHECK_STATE( oSig );

        CHECK_STATE( ECDSA_SIG_set0( oSig, rBN, sBN ) != 0 );

        CHECK_STATE(
            ECDSA_do_verify( ( const unsigned char* ) _hash, 32, oSig, this->ecKey ) == 1 );

        returnValue = true;

    } catch ( exception& e ) {
        LOG( err, "ECDSA sig did not verify: exception" + string( e.what() ) );
        returnValue = false;
    }

clean:

    return returnValue;
}

bool OpenSSLECDSAKey::sessionVerifySig( ptr< string > _signature, const char* _hash ) {
    CHECK_ARGUMENT( _signature );
    CHECK_ARGUMENT( _hash );

    CHECK_STATE( _signature->size() % 2 == 0 );

    vector< unsigned char > derSig( _signature->size() / 2 );

    Utils::cArrayFromHex( *_signature, derSig.data(), derSig.size() );

    auto p = ( const unsigned char* ) derSig.data();

    ECDSA_SIG* sig = d2i_ECDSA_SIG( nullptr, &p, ( long ) derSig.size() );

    CHECK_STATE( sig );

    auto status = ECDSA_do_verify( ( const unsigned char* ) _hash, 32, sig, ecKey );

    CHECK_STATE( status >= 0 );

    return status == 1;
}


ptr< string > OpenSSLECDSAKey::sessionSign( const char* _hash ) {
    CHECK_ARGUMENT( _hash );
    CHECK_STATE( ecKey );
    CHECK_STATE( isPrivate );

    ECDSA_SIG* signature = nullptr;
    ptr<string> hexSig = nullptr;

    try {
        signature = ECDSA_do_sign( ( const unsigned char* ) _hash, 32, ecKey );
        CHECK_STATE( signature );
        uint64_t sigLen = i2d_ECDSA_SIG( signature, nullptr );
        vector< unsigned char > sigDer( sigLen, 0 );
        auto pointer = sigDer.data();

        CHECK_STATE( i2d_ECDSA_SIG( signature, &( pointer ) ) > 0 );
        hexSig = Utils::carray2Hex( sigDer.data(), sigLen );
        CHECK_STATE( hexSig );
    } catch ( ... ) {
        if (signature) ECDSA_SIG_free(signature);
        throw;
    }
    if (signature) ECDSA_SIG_free(signature);
    return hexSig;
}

ptr< OpenSSLECDSAKey > OpenSSLECDSAKey::makeKey( ptr< string > _publicKey, bool _isSGX ) {
    CHECK_ARGUMENT( _publicKey );


    if ( ecgroup == nullptr ) {
        ecgroup = EC_GROUP_new_by_curve_name( NID_secp256k1 );
        CHECK_STATE( ecgroup );
    }

    EC_KEY* pubKey = nullptr;
    BIGNUM* xBN = nullptr;
    BIGNUM* yBN = nullptr;
    EC_POINT* point = nullptr;

    try {
        pubKey = EC_KEY_new_by_curve_name( NID_secp256k1 );
        CHECK_STATE( pubKey );
        if ( _isSGX ) {
            auto x = _publicKey->substr( 0, 64 );
            auto y = _publicKey->substr( 64, 128 );
            xBN = BN_new();
            yBN = BN_new();
            CHECK_STATE( BN_hex2bn( &xBN, x.c_str() ) != 0 );
            CHECK_STATE( BN_hex2bn( &yBN, y.c_str() ) != 0 );
            CHECK_STATE( EC_KEY_set_public_key_affine_coordinates( pubKey, xBN, yBN ) == 1 );
        } else {
            point = EC_POINT_hex2point( ecgroup, _publicKey->c_str(), nullptr, nullptr );
            CHECK_STATE( point );
            CHECK_STATE( EC_KEY_set_public_key( pubKey, point ) == 1 );
        }
    } catch ( ... ) {
        if ( pubKey )
            EC_KEY_free( pubKey );
        if ( xBN )
            BN_free( xBN );
        if ( yBN )
            BN_free( yBN );
        if ( point )
            EC_POINT_clear_free( point );
        throw;
    }

    if ( xBN )
        BN_free( xBN );
    if ( yBN )
        BN_free( yBN );
    if ( point )
        EC_POINT_clear_free( point );

    return make_shared< OpenSSLECDSAKey >( pubKey, false );
}