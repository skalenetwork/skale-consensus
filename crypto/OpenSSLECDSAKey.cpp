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

OpenSSLECDSAKey::OpenSSLECDSAKey( EC_KEY* _ecKey ) {
    CHECK_STATE( _ecKey );
    this->ecKey = _ecKey;
    isPrivate = true;
}
OpenSSLECDSAKey::~OpenSSLECDSAKey() {
    if ( ecKey )
        EC_KEY_free( ecKey );
}
ptr< OpenSSLECDSAKey > OpenSSLECDSAKey::generateKey() {
    EC_KEY* eckey = EC_KEY_new_by_curve_name( NID_secp256k1 );
    CHECK_STATE( eckey );


    if ( ecgroup == nullptr ) {
        ecgroup = EC_GROUP_new_by_curve_name( NID_secp256k1 );
        CHECK_STATE( ecgroup );
    }

    CHECK_STATE( EC_KEY_set_group( eckey, ecgroup ) == 1 );

    CHECK_STATE( EC_KEY_generate_key( eckey ) == 1 )

    CHECK_STATE( eckey );

    return make_shared< OpenSSLECDSAKey >( eckey );
}


EC_GROUP* OpenSSLECDSAKey::ecgroup = nullptr;

EC_KEY* OpenSSLECDSAKey::getEcKey() const {
    CHECK_STATE( ecKey );
    return ecKey;
}

ptr< string > OpenSSLECDSAKey::getPublicKey() {
    auto pubKeyComponent = EC_KEY_get0_public_key( ecKey );

    CHECK_STATE( pubKeyComponent );

    if ( ecgroup == nullptr ) {
        ecgroup = EC_GROUP_new_by_curve_name( NID_secp256k1 );
        CHECK_STATE( ecgroup );
    }

    auto hex = EC_POINT_point2hex( ecgroup, pubKeyComponent, POINT_CONVERSION_COMPRESSED, NULL );

    CHECK_STATE( hex );

    auto result = make_shared< string >( hex );

    OPENSSL_free( hex );

    return result;
}

bool OpenSSLECDSAKey::verifyHash( ptr< string > _signature, const char* _hash ) {
    CHECK_ARGUMENT( _signature );
    CHECK_ARGUMENT( _hash );

    CHECK_STATE( _signature->size() % 2 == 0 );

    auto len = _signature->size() / 2;

    vector< unsigned char > derSig( len );

    Utils::cArrayFromHex( *_signature, derSig.data(), derSig.size() );

    auto p = ( const unsigned char* ) derSig.data();

    ECDSA_SIG* sig = d2i_ECDSA_SIG( nullptr, &p, ( long ) derSig.size() );

    CHECK_STATE( sig );

    auto status = ECDSA_do_verify( ( const unsigned char* ) _hash, 32, sig, ecKey );

    CHECK_STATE( status >= 0 );

    return status == 1;
}


ptr< string > OpenSSLECDSAKey::signHash( const char* _hash ) {
    CHECK_ARGUMENT( _hash );
    CHECK_STATE( ecKey );
    CHECK_STATE( isPrivate );

    ECDSA_SIG* signature = ECDSA_do_sign( ( const unsigned char* ) _hash, 32, ecKey );

    CHECK_STATE( signature );

    uint64_t sigLen = i2d_ECDSA_SIG( signature, nullptr );

    vector< unsigned char > sigDer( sigLen, 0 );

    auto pointer = sigDer.data();

    CHECK_STATE( i2d_ECDSA_SIG( signature, &( pointer ) ) > 0 );

    auto hexSig = Utils::carray2Hex( sigDer.data(), sigLen );

    CHECK_STATE( hexSig );

    return hexSig;
}
OpenSSLECDSAKey::OpenSSLECDSAKey( ptr< string > _publicKey, bool _isSGX ) {
    CHECK_ARGUMENT( _publicKey );

    isPrivate = false;

    if ( ecgroup == nullptr ) {
        ecgroup = EC_GROUP_new_by_curve_name( NID_secp256k1 );
        CHECK_STATE( ecgroup );
    }

    auto pubKey = EC_KEY_new_by_curve_name( NID_secp256k1 );
    CHECK_STATE( pubKey );
    if ( _isSGX ) {
        auto x = _publicKey->substr( 0, 64 );
        auto y = _publicKey->substr( 64, 128 );
        auto xBN = BN_new();
        auto yBN = BN_new();
        CHECK_STATE( BN_hex2bn( &xBN, x.c_str() ) != 0 );
        CHECK_STATE( BN_hex2bn( &yBN, y.c_str() ) != 0 );
        CHECK_STATE( EC_KEY_set_public_key_affine_coordinates( pubKey, xBN, yBN ) == 1 );
    } else {
        auto point = EC_POINT_hex2point( ecgroup, _publicKey->c_str(), nullptr, nullptr );
        CHECK_STATE( point );
        CHECK_STATE( EC_KEY_set_public_key( pubKey, point ) == 1 );
        EC_POINT_clear_free( point );
    }

    this->ecKey = pubKey;
}