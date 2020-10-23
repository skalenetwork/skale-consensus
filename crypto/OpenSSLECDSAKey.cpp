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
#include <openssl/x509.h>
#include <openssl/ec.h>  // for EC_GROUP_new_by_curve_name, EC_GROUP_free, EC_KEY_new, EC_KEY_set_group, EC_KEY_generate_key, EC_KEY_free
#include <openssl/ecdsa.h>    // for ECDSA_do_sign, ECDSA_do_verify
#include <openssl/obj_mac.h>  // for NID_secp256k1


#include "openssl/bn.h"
#include "openssl/ecdsa.h"
#include "openssl/sha.h"
#include <openssl/evp.h>
#include <openssl/pem.h>

#include "Log.h"

#include "OpenSSLECDSAKey.h"
#include "network/Utils.h"

#include "SkaleCommon.h"

#define NID_FAST NID_X9_62_prime256v1
#define NID_ETH NID_secp256k1

OpenSSLECDSAKey::OpenSSLECDSAKey( EC_KEY* _ecKey, EVP_PKEY* _edKey, bool _isPrivate, bool _isFast )
    : isPrivate( _isPrivate ), isFast( _isFast ) {
    CHECK_STATE( _ecKey );
    CHECK_STATE( _ecKey || _edKey );
    this->ecKey = _ecKey;
    this->edKey = _edKey;
}
OpenSSLECDSAKey::~OpenSSLECDSAKey() {
    if ( ecKey )
        EC_KEY_free( ecKey );
    if ( edKey ) {
        EVP_PKEY_free( edKey );
    }
}


ptr< OpenSSLECDSAKey > OpenSSLECDSAKey::generateKey( bool _isFast ) {
    initGroupsIfNeeded();

    EC_KEY* eckey = nullptr;
    EVP_PKEY* edkey = nullptr;

    int nid = ( _isFast ) ? NID_FAST : NID_ETH;

    if ( _isFast ) {
        edkey = genFastKey();
    }

    eckey = generateETHKey( nid );

    return make_shared< OpenSSLECDSAKey >( eckey, edkey, true, _isFast );
}
EC_KEY* OpenSSLECDSAKey::generateETHKey( int nid ) {
    EC_KEY* eckey = nullptr;
    try {
        eckey = EC_KEY_new_by_curve_name( nid );
        CHECK_STATE( eckey );
        CHECK_STATE( EC_KEY_generate_key( eckey ) == 1 )
    } catch ( ... ) {
        if ( eckey ) {
            EC_KEY_free( eckey );
        }
        throw;
    }
    return eckey;
}
EVP_PKEY* OpenSSLECDSAKey::genFastKey() {
    EVP_PKEY* edkey = nullptr;
    EVP_PKEY_CTX* ctx = nullptr;
    try {
        ctx = EVP_PKEY_CTX_new_id( NID_ED25519, NULL );
        CHECK_STATE( ctx );
        EVP_PKEY_keygen_init( ctx );
        edkey = EVP_PKEY_new();
        CHECK_STATE( edkey );
        CHECK_STATE( EVP_PKEY_keygen( ctx, &edkey ) > 0 );

    } catch ( ... ) {
        if ( ctx ) {
            EVP_PKEY_CTX_free( ctx );
        }
        if ( edkey ) {
            EVP_PKEY_free( edkey );
        }
        throw;
    }

    if ( ctx ) {
        EVP_PKEY_CTX_free( ctx );
    }

    return edkey;
}
void OpenSSLECDSAKey::initGroupsIfNeeded() {
    if ( ecgroup == nullptr ) {
        ecgroup = EC_GROUP_new_by_curve_name( NID_ETH );
        ecgroupFast = EC_GROUP_new_by_curve_name( NID_FAST );
        CHECK_STATE( ecgroup );
        CHECK_STATE( ecgroupFast );
    }
}

EC_GROUP* OpenSSLECDSAKey::ecgroup = nullptr;
EC_GROUP* OpenSSLECDSAKey::ecgroupFast = nullptr;

string OpenSSLECDSAKey::getPublicKey() {
    initGroupsIfNeeded();

    auto pubKeyComponent = EC_KEY_get0_public_key( ecKey );

    CHECK_STATE( pubKeyComponent );

    char* hex = nullptr;
    string result = "";

    try {
        if ( this->isFast ) {
            hex = EC_POINT_point2hex(
                ecgroupFast, pubKeyComponent, POINT_CONVERSION_COMPRESSED, NULL );
        } else {
            hex = EC_POINT_point2hex( ecgroup, pubKeyComponent, POINT_CONVERSION_COMPRESSED, NULL );
        }

        CHECK_STATE( hex );

        result = hex;

    } catch ( ... ) {
        if ( hex )
            OPENSSL_free( hex );
        throw;
    }

    if ( hex )
        OPENSSL_free( hex );

    return result;
}

bool OpenSSLECDSAKey::verifySGXSig( const string& _sig, const char* _hash ) {
    bool returnValue = false;
    BIGNUM* rBN = BN_new();
    BIGNUM* sBN = BN_new();
    ECDSA_SIG* oSig = nullptr;
    CHECK_STATE( rBN );
    CHECK_STATE( sBN );
    string r, s;
    uint64_t firstColumn = 0, secondColumn = 0;

    firstColumn = _sig.find( ":" );

    if ( firstColumn == string::npos || firstColumn == _sig.length() - 1 ) {
        LOG( warn, "Misfomatted signature" );
        goto clean;
    }

    secondColumn = _sig.find( ":", firstColumn + 1 );

    if ( secondColumn == string::npos || secondColumn == _sig.length() - 1 ) {
        LOG( warn, "Misformatted signature 2" );
        goto clean;
    }

    r = _sig.substr( firstColumn + 1, secondColumn - firstColumn - 1 );
    s = _sig.substr( secondColumn + 1, _sig.length() - secondColumn - 1 );

    if ( r == s ) {
        LOG( warn, "r == s " );
        goto clean;
    }

    CHECK_STATE( firstColumn != secondColumn );


    if ( BN_hex2bn( &rBN, r.c_str() ) == 0 ) {
        LOG( warn, "BN_hex2bn( &rBN, r.c_str() ) == 0" );
        goto clean;
    };

    if ( BN_hex2bn( &sBN, s.c_str() ) == 0 ) {
        LOG( warn, "BN_hex2bn( &sBN, s.c_str() ) == 0" );
        goto clean;
    };

    oSig = ECDSA_SIG_new();

    if ( ECDSA_SIG_set0( oSig, rBN, sBN ) == 0 ) {
        LOG( warn, "ECDSA_SIG_set0( oSig, rBN, sBN ) == 0" );
        goto clean;
    }

    returnValue = ECDSA_do_verify( ( const unsigned char* ) _hash, 32, oSig, this->ecKey ) == 1;

clean:

    if ( oSig ) {
        ECDSA_SIG_free( oSig );
    } else {
        if ( rBN )
            BN_free( rBN );
        if ( sBN )
            BN_free( sBN );
    }

    return returnValue;
}

bool OpenSSLECDSAKey::sessionVerifySig( const string& _signature, const char* _hash ) {
    CHECK_ARGUMENT( _signature != "" );
    CHECK_ARGUMENT( _hash );

    if ( _signature.size() % 2 != 0 )
        return false;

    vector< unsigned char > derSig( _signature.size() / 2 );

    Utils::cArrayFromHex( _signature, derSig.data(), derSig.size() );

    auto p = ( const unsigned char* ) derSig.data();

    ECDSA_SIG* sig = d2i_ECDSA_SIG( nullptr, &p, ( long ) derSig.size() );

    if ( !sig ) {
        return false;
    }

    auto status = ECDSA_do_verify( ( const unsigned char* ) _hash, 32, sig, ecKey );

    if ( sig )
        ECDSA_SIG_free( sig );

    return status == 1;
}


string OpenSSLECDSAKey::sessionSign( const char* _hash ) {
    CHECK_ARGUMENT( _hash );
    CHECK_STATE( ecKey );
    CHECK_STATE( isPrivate );

    ECDSA_SIG* signature = nullptr;
    string hexSig = "";

    fastSign( _hash );

    try {
        signature = ECDSA_do_sign( ( const unsigned char* ) _hash, 32, ecKey );
        CHECK_STATE( signature );
        uint64_t sigLen = i2d_ECDSA_SIG( signature, nullptr );
        vector< unsigned char > sigDer( sigLen, 0 );
        auto pointer = sigDer.data();

        CHECK_STATE( i2d_ECDSA_SIG( signature, &( pointer ) ) > 0 );
        hexSig = Utils::carray2Hex( sigDer.data(), sigLen );
        CHECK_STATE( hexSig != "" );
    } catch ( ... ) {
        if ( signature )
            ECDSA_SIG_free( signature );
        throw;
    }
    if ( signature )
        ECDSA_SIG_free( signature );
    return hexSig;
}



void OpenSSLECDSAKey::fastSign( const char* _hash ) {
    EVP_MD_CTX* ctx = nullptr;

    try {
        ctx = EVP_MD_CTX_new();

        CHECK_STATE( ctx )

        CHECK_STATE( edKey )

        CHECK_STATE( EVP_DigestSignInit( ctx, NULL, NULL, NULL, edKey ) > 0 )


        size_t len = 0;

        CHECK_STATE( EVP_DigestSign( ctx, nullptr, &len, ( const unsigned char* ) _hash, 32 ) > 0 );

        vector< unsigned char > sig( len, 0 );

        CHECK_STATE(
            EVP_DigestSign( ctx, sig.data(), &len, ( const unsigned char* ) _hash, 32 ) > 0 )

        vector< unsigned char > encodedSig( 2 * len + 1, 0 );

        auto encodedLen = EVP_EncodeBlock( encodedSig.data(), sig.data(), len );
        CHECK_STATE( encodedLen > 10 );
        string encodedSignature( ( const char* ) encodedSig.data() );


        // now encode key

        auto encodedPubKeyStr =  getFastPubKey();

        // now decode key


        EVP_PKEY* pubKey = decodePubKey( encodedPubKeyStr );


        auto t = edKey;
        edKey = pubKey;

        // verify

        CHECK_STATE(verifyFastSig( _hash, encodedSignature ));

        edKey = t;

    } catch ( ... ) {
        if ( ctx ) {
            EVP_MD_CTX_free( ctx );
        }

        throw;
    }

    if ( ctx ) {
        EVP_MD_CTX_free( ctx );
    }
}
EVP_PKEY* OpenSSLECDSAKey::decodePubKey( string& encodedPubKeyStr ) const {

    EVP_PKEY* pubKey = nullptr;
    BIO * encodedPubKeyBio = nullptr;

    try {
        CHECK_STATE( !encodedPubKeyStr.empty() );

        encodedPubKeyBio = BIO_new_mem_buf( encodedPubKeyStr.data(), encodedPubKeyStr.size() );

        CHECK_STATE( encodedPubKeyBio );

        pubKey = PEM_read_bio_PUBKEY( encodedPubKeyBio, nullptr, nullptr, nullptr );

        CHECK_STATE( pubKey );

    } catch (...) {
        if (encodedPubKeyBio) {
            BIO_free(encodedPubKeyBio);
        }
        throw;
    }

    if (encodedPubKeyBio) {
        BIO_free(encodedPubKeyBio);
    }

    return pubKey;
}

string OpenSSLECDSAKey::getFastPubKey() const {
    BIO * bio = nullptr;
    string result;
    try {
        bio = BIO_new( BIO_s_mem() );
        CHECK_STATE( bio );
        CHECK_STATE(edKey);
        CHECK_STATE( PEM_write_bio_PUBKEY( bio, edKey ) );

        char* encodedPubKey = nullptr;
        auto pubKeyEncodedLen = BIO_get_mem_data( bio, &encodedPubKey );
        CHECK_STATE( pubKeyEncodedLen > 10 );
        result = string( encodedPubKey, 0, pubKeyEncodedLen );

    } catch (...) {
        if (bio) {
            BIO_free(bio);
        }
        throw;
    }

    if (bio) {
        BIO_free(bio);
    }
    return result;
}
bool OpenSSLECDSAKey::verifyFastSig( const char* _hash, const string& _encodedSignature ) const {
    CHECK_STATE( _hash );

    bool result = false;

    EVP_MD_CTX *  verifyCtx = nullptr;

    try {

        verifyCtx = EVP_MD_CTX_new();

        vector< unsigned char > decodedSig( _encodedSignature.size(), 0 );

        int decodedLen = 0;

        CHECK_STATE( ( decodedLen = EVP_DecodeBlock( decodedSig.data(),
                           ( const unsigned char* ) _encodedSignature.c_str(),
                           _encodedSignature.size() ) ) > 0 )

        CHECK_STATE( decodedLen >= 64 )



        CHECK_STATE( verifyCtx );

        CHECK_STATE( EVP_DigestVerifyInit( verifyCtx, NULL, NULL, NULL, edKey ) > 0 )

        CHECK_STATE( EVP_DigestVerify( verifyCtx, decodedSig.data(), 64,
                         ( const unsigned char* ) _hash, 32 ) == 1 );
        result = true;
    } catch (...) {
        if (!verifyCtx) { // out of memory
            throw;
        }
    }

    if (verifyCtx) {
        EVP_MD_CTX_free(verifyCtx);
    }

    return result;

}

ptr< OpenSSLECDSAKey > OpenSSLECDSAKey::importPubKey( const string& _publicKey) {
    CHECK_ARGUMENT( _publicKey != "" );
    initGroupsIfNeeded();

    EC_KEY* pubKey = nullptr;
    BIGNUM* xBN = nullptr;
    BIGNUM* yBN = nullptr;
    EC_POINT* point = nullptr;

    try {

            pubKey = EC_KEY_new_by_curve_name( NID_ETH );
            CHECK_STATE( pubKey );
            auto x = _publicKey.substr( 0, 64 );
            auto y = _publicKey.substr( 64, 128 );
            xBN = BN_new();
            yBN = BN_new();
            CHECK_STATE( BN_hex2bn( &xBN, x.c_str() ) != 0 );
            CHECK_STATE( BN_hex2bn( &yBN, y.c_str() ) != 0 );
            CHECK_STATE( EC_KEY_set_public_key_affine_coordinates( pubKey, xBN, yBN ) == 1 );

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

    return make_shared< OpenSSLECDSAKey >( pubKey, nullptr, false, false );
}


ptr< OpenSSLECDSAKey > OpenSSLECDSAKey::importPubKeyFast( const string& _publicKey) {
    CHECK_ARGUMENT( _publicKey != "" );
    initGroupsIfNeeded();

    EC_KEY* pubKey = nullptr;
    BIGNUM* xBN = nullptr;
    BIGNUM* yBN = nullptr;
    EC_POINT* point = nullptr;

    try {
            pubKey = EC_KEY_new_by_curve_name( NID_FAST );
            CHECK_STATE( pubKey );
            point = EC_POINT_hex2point( ecgroupFast, _publicKey.c_str(), nullptr, nullptr );
            CHECK_STATE( point );
            CHECK_STATE( EC_KEY_set_public_key( pubKey, point ) == 1 );
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

    return make_shared< OpenSSLECDSAKey >( pubKey, nullptr, false, true );
}