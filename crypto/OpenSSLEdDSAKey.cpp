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
#include "network/Utils.h"
#include "SkaleCommon.h"
#include "OpenSSLEdDSAKey.h"
#define NID_FAST NID_X9_62_prime256v1
#define NID_ETH NID_secp256k1

OpenSSLEdDSAKey::OpenSSLEdDSAKey( EVP_PKEY* _edKey, bool _isPrivate ) : isPrivate( _isPrivate ) {
    CHECK_STATE( _edKey );

    this->edKey = _edKey;
}
OpenSSLEdDSAKey::~OpenSSLEdDSAKey() {
    if ( edKey ) {
        EVP_PKEY_free( edKey );
    }
}


ptr< OpenSSLEdDSAKey > OpenSSLEdDSAKey::generateKey() {
    EVP_PKEY* edkey = nullptr;

    edkey = genFastKeyImpl();

    return make_shared< OpenSSLEdDSAKey >( edkey, true );
}


EVP_PKEY* OpenSSLEdDSAKey::genFastKeyImpl() {
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


string OpenSSLEdDSAKey::sign( const char* _hash ) {
    CHECK_ARGUMENT( _hash );
    CHECK_STATE( this->edKey )
    CHECK_STATE( isPrivate );
    string fastSig = fastSignImpl( _hash );
    return fastSig;
}


string OpenSSLEdDSAKey::fastSignImpl( const char* _hash ) {
    EVP_MD_CTX* ctx = nullptr;

    string encodedSignature;

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
        encodedSignature = string( ( const char* ) encodedSig.data() );

    } catch ( ... ) {
        if ( ctx ) {
            EVP_MD_CTX_free( ctx );
        }

        throw;
    }

    if ( ctx ) {
        EVP_MD_CTX_free( ctx );
    }
    return encodedSignature;
}

EVP_PKEY* OpenSSLEdDSAKey::deserializeFastPubKey( const string& encodedPubKeyStr ) {
    EVP_PKEY* pubKey = nullptr;
    BIO* encodedPubKeyBio = nullptr;

    try {
        CHECK_STATE( !encodedPubKeyStr.empty() );

        encodedPubKeyBio = BIO_new_mem_buf( encodedPubKeyStr.data(), encodedPubKeyStr.size() );

        CHECK_STATE( encodedPubKeyBio );

        pubKey = PEM_read_bio_PUBKEY( encodedPubKeyBio, nullptr, nullptr, nullptr );

        CHECK_STATE( pubKey );

    } catch ( ... ) {
        if ( encodedPubKeyBio ) {
            BIO_free( encodedPubKeyBio );
        }
        throw;
    }

    if ( encodedPubKeyBio ) {
        BIO_free( encodedPubKeyBio );
    }

    return pubKey;
}

string OpenSSLEdDSAKey::serializePubKey() const {
    BIO* bio = nullptr;
    string result;
    try {
        bio = BIO_new( BIO_s_mem() );
        CHECK_STATE( bio );
        CHECK_STATE( edKey );
        CHECK_STATE( PEM_write_bio_PUBKEY( bio, edKey ) );

        char* encodedPubKey = nullptr;
        auto pubKeyEncodedLen = BIO_get_mem_data( bio, &encodedPubKey );

        CHECK_STATE( pubKeyEncodedLen > 10 );
        result = string( encodedPubKey, pubKeyEncodedLen );

    } catch ( ... ) {
        if ( bio ) {
            BIO_free( bio );
        }
        throw;
    }

    if ( bio ) {
        BIO_free( bio );
    }
    return result;
}

void OpenSSLEdDSAKey::verifySig( const string& _encodedSignature, const char* _hash ) const {
    CHECK_STATE( _hash );

    EVP_MD_CTX* verifyCtx = nullptr;

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

    } catch ( ... ) {
        if ( verifyCtx ) {
            EVP_MD_CTX_free( verifyCtx );
        }
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }


    if ( verifyCtx )
        EVP_MD_CTX_free( verifyCtx );
}


ptr< OpenSSLEdDSAKey > OpenSSLEdDSAKey::importPubKey( const string& _publicKey ) {
    auto pubKey = deserializeFastPubKey( _publicKey );
    return make_shared< OpenSSLEdDSAKey >( pubKey, false );
}
