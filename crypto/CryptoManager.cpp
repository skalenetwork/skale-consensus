/*
    Copyright (C) 2019 SKALE Labs

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

    @file CryptoManager.h
    @author Stan Kladko
    @date 2019

*/


#include "openssl/bio.h"

#include "openssl/ec.h"
#include "openssl/err.h"
#include "openssl/evp.h"
#include "openssl/pem.h"

#include <cryptopp/eccrypto.h>
#include <cryptopp/hex.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>
#include <cryptopp/sha.h>

#include "JsonStubClient.h"


#include <jsonrpccpp/client/connectors/httpclient.h>

#include <openssl/ec.h>  // for EC_GROUP_new_by_curve_name, EC_GROUP_free, EC_KEY_new, EC_KEY_set_group, EC_KEY_generate_key, EC_KEY_free
#include <openssl/ecdsa.h>    // for ECDSA_do_sign, ECDSA_do_verify
#include <openssl/obj_mac.h>  // for NID_secp192k1

#include "Log.h"
#include "SkaleCommon.h"

#include <gmp.h>

#include "ConsensusBLSSigShare.h"
#include "ConsensusBLSSignature.h"
#include "ConsensusSigShareSet.h"
#include "MockupSigShare.h"
#include "MockupSigShareSet.h"
#include "MockupSignature.h"
#include "SHAHash.h"

#include "chains/Schain.h"
#include "messages/NetworkMessage.h"

#include "bls/BLSPrivateKeyShare.h"
#include "datastructures/BlockProposal.h"
#include "monitoring/LivelinessMonitor.h"
#include "node/Node.h"
#include "node/NodeInfo.h"

#include "json/JSONFactory.h"

#include "network/Utils.h"

#include "CryptoManager.h"
#include "OpenSSLECDSAKey.h"


void CryptoManager::initSGXClient() {
    if ( isSGXEnabled ) {
        if ( isHTTPSEnabled ) {
            if ( sgxSSLKeyFileFullPath && sgxSSLCertFileFullPath ) {
                LOG( info, string( "Setting sgxSSLKeyFileFullPath to " ) + *sgxSSLKeyFileFullPath );
                LOG( info,
                    string( "Setting sgxCertKeyFileFullPath to " ) + *sgxSSLCertFileFullPath );
                setSGXKeyAndCert(
                    *sgxSSLKeyFileFullPath, *sgxSSLCertFileFullPath, parseSGXPort( sgxURL ) );
            } else {
                LOG( info, string( "Setting sgxSSLKeyCertFileFullPath  is not set."
                                   "Assuming SGX server does not require client certs" ) );
            }
        }

        httpClient = make_shared< jsonrpc::HttpClient >( *sgxURL );
        sgxClient = make_shared< StubClient >( *httpClient, jsonrpc::JSONRPC_CLIENT_V2 );
    }
}
ptr< BLSPublicKey > CryptoManager::getSgxBlsPublicKey() {
    CHECK_STATE( sgxBLSPublicKey );
    return sgxBLSPublicKey;
}

ptr< string > CryptoManager::getSgxBlsKeyName() {
    CHECK_STATE( sgxBlsKeyName );
    return sgxBlsKeyName;
}

CryptoManager::CryptoManager( uint64_t _totalSigners, uint64_t _requiredSigners, bool _isSGXEnabled,
    ptr< string > _sgxURL, ptr< string > _sgxSslKeyFileFullPath,
    ptr< string > _sgxSslCertFileFullPath, ptr< string > _sgxEcdsaKeyName,
    ptr< vector< string > > _sgxEcdsaPublicKeys )
    : sessionKeys( SESSION_KEY_CACHE_SIZE ) {
    CHECK_ARGUMENT( _totalSigners >= _requiredSigners );
    totalSigners = _totalSigners;
    requiredSigners = _requiredSigners;

    isSGXEnabled = _isSGXEnabled;

    if ( _isSGXEnabled ) {
        CHECK_ARGUMENT( _sgxURL );
        CHECK_ARGUMENT( _sgxEcdsaKeyName );
        CHECK_ARGUMENT( _sgxEcdsaPublicKeys );

        sgxURL = _sgxURL;
        sgxSSLKeyFileFullPath = _sgxSslKeyFileFullPath;
        sgxSSLCertFileFullPath = _sgxSslCertFileFullPath;
        sgxECDSAKeyName = _sgxEcdsaKeyName;
        sgxECDSAPublicKeys = _sgxEcdsaPublicKeys;
    }

    initSGXClient();
}


uint64_t CryptoManager::parseSGXPort( ptr< string > _url ) {
    CHECK_ARGUMENT( _url );
    size_t found = _url->find_first_of( ":" );

    if ( found == string::npos ) {
        BOOST_THROW_EXCEPTION(
            InvalidStateException( "SGX URL does not include port " + *_url, __CLASS_NAME__ ) );
    }


    string end = _url->substr( found + 1 );

    size_t found1 = end.find_first_of( ":" );

    if ( found1 == string::npos ) {
        found1 = end.size();
    }


    string port = end.substr( found1 + 1, end.size() - found1 );


    uint64_t result;

    try {
        result = stoi( port );
    } catch ( ... ) {
        throw_with_nested(
            InvalidStateException( "Could not find port in URL " + *_url, __CLASS_NAME__ ) );
    }
    return result;
}


CryptoManager::CryptoManager( Schain& _sChain )
    : sessionKeys( SESSION_KEY_CACHE_SIZE ), sChain( &_sChain ) {
    totalSigners = getSchain()->getTotalSigners();
    requiredSigners = getSchain()->getRequiredSigners();

    CHECK_ARGUMENT( totalSigners >= requiredSigners );

    isSGXEnabled = _sChain.getNode()->isSgxEnabled();

    if ( isSGXEnabled ) {
        auto node = _sChain.getNode();
        sgxURL = node->getSgxUrl();
        sgxSSLCertFileFullPath = node->getSgxSslCertFileFullPath();
        sgxSSLKeyFileFullPath = node->getSgxSslKeyFileFullPath();
        sgxECDSAKeyName = node->getEcdsaKeyName();
        sgxECDSAPublicKeys = node->getEcdsaPublicKeys();
        sgxBlsKeyName = node->getBlsKeyName();
        sgxBLSPublicKeys = node->getBlsPublicKeys();
        sgxBLSPublicKey = node->getBlsPublicKey();


        CHECK_STATE( sgxURL );
        CHECK_STATE( sgxECDSAKeyName );
        CHECK_STATE( sgxECDSAPublicKeys );
        CHECK_STATE( sgxBlsKeyName );
        CHECK_STATE( sgxBLSPublicKeys );
        CHECK_STATE( sgxBLSPublicKey );

        CHECK_STATE( JSONFactory::splitString( *sgxBlsKeyName )->size() == 7 );
        CHECK_STATE( JSONFactory::splitString( *sgxECDSAKeyName )->size() == 2 );

        isHTTPSEnabled = sgxURL->find( "https:/" ) != string::npos;


        initSGXClient();


        for ( uint64_t i = 0; i < ( uint64_t ) getSchain()->getNodeCount(); i++ ) {
            auto nodeId = getSchain()->getNode()->getNodeInfoByIndex( i + 1 )->getNodeID();
            ecdsaPublicKeyMap[( uint64_t ) nodeId] =
                make_shared< string >( sgxECDSAPublicKeys->at( i ) );
            blsPublicKeyMap[( uint64_t ) nodeId] = sgxBLSPublicKeys->at( i );

            if ( nodeId == getSchain()->getThisNodeInfo()->getNodeID() ) {
                auto publicKey = getSGXEcdsaPublicKey( sgxECDSAKeyName, this->sgxClient );
                if ( *publicKey != sgxECDSAPublicKeys->at( i ) ) {
                    BOOST_THROW_EXCEPTION( InvalidStateException(
                        "Misconfiguration. \n Configured ECDSA public key for this node \n" +
                            sgxECDSAPublicKeys->at( i ) +
                            " \n is not equal to the public key for \n " + *sgxECDSAKeyName +
                            "\n  on the SGX server: \n" + *publicKey,
                        __CLASS_NAME__ ) );
                };
            }
        }


        try {
            blsPublicKeyObj = getSgxBlsPublicKey();
        } catch ( ... ) {
            throw_with_nested(
                InvalidStateException( "Could not create blsPublicKey", __CLASS_NAME__ ) );
        }
    }
}

void CryptoManager::setSGXKeyAndCert(
    string& _keyFullPath, string& _certFullPath, uint64_t _sgxPort ) {
    jsonrpc::HttpClient::setKeyFileFullPath( _keyFullPath );
    jsonrpc::HttpClient::setCertFileFullPath( _certFullPath );
    jsonrpc::HttpClient::setSslClientPort( _sgxPort );
}

Schain* CryptoManager::getSchain() const {
    return sChain;
}


static domain_parameters ecdsaCurve = NULL;

#define ECDSA_SKEY_LEN 65
#define ECDSA_SKEY_BASE 16

#define BUF_SIZE 1024
#define PUB_KEY_SIZE 64


MPZNumber::MPZNumber() {
    mpz_init( this->number );
}

MPZNumber::~MPZNumber() {
    mpz_clear( this->number );
}

using namespace std;
unsigned long long int random_value = 0;  // Declare value to store data into
size_t size = sizeof( random_value );     // Declare size of data


static ifstream urandom( "/dev/urandom", ios::in | ios::binary );  // Open stream

std::tuple< ptr< MPZNumber >, ptr< string > > CryptoManager::localGenerateEcdsaKey() {
    domain_parameters curve = domain_parameters_init();
    domain_parameters_load_curve( curve, secp256k1 );

    unsigned char* rand_char = ( unsigned char* ) calloc( 32, 1 );

    CHECK_STATE( urandom );
    urandom.read( reinterpret_cast< char* >( rand_char ), 32 );  // Read from urandom
    CHECK_STATE( urandom );


    mpz_t seed;
    mpz_init( seed );
    mpz_import( seed, 32, 1, sizeof( rand_char[0] ), 0, 0, rand_char );

    free( rand_char );

    auto sKey = make_shared< MPZNumber >();

    mpz_mod( sKey->number, seed, curve->p );
    mpz_clear( seed );

    // Public key
    point Pkey = point_init();

    signature_extract_public_key( Pkey, sKey->number, curve );

    char pub_key_x[BUF_SIZE];
    char pub_key_y[BUF_SIZE];

    char arr_x[mpz_sizeinbase( Pkey->x, ECDSA_SKEY_BASE ) + 2];
    mpz_get_str( arr_x, ECDSA_SKEY_BASE, Pkey->x );
    int n_zeroes = PUB_KEY_SIZE - strlen( arr_x );
    for ( int i = 0; i < n_zeroes; i++ ) {
        pub_key_x[i] = '0';
    }
    strncpy( pub_key_x + n_zeroes, arr_x, BUF_SIZE - n_zeroes );

    char arr_y[mpz_sizeinbase( Pkey->y, ECDSA_SKEY_BASE ) + 2];
    mpz_get_str( arr_y, ECDSA_SKEY_BASE, Pkey->y );
    n_zeroes = PUB_KEY_SIZE - strlen( arr_y );
    for ( int i = 0; i < n_zeroes; i++ ) {
        pub_key_y[i] = '0';
    }
    strncpy( pub_key_y + n_zeroes, arr_y, BUF_SIZE - n_zeroes );


    auto pKey = make_shared< string >( string( pub_key_x ) + string( pub_key_y ) );

    // mpz_clear(skey);
    domain_parameters_clear( curve );
    point_clear( Pkey );

    return { sKey, pKey };
}

void CryptoManager::signature_sign(
    signature sig, mpz_t message, mpz_t private_key, domain_parameters curve ) {
    // message must not have a bit length longer than that of n
    // see: Guide to Elliptic Curve Cryptography, section 4.4.1.

    for ( int i = 0; i < 1; i++ ) {
        assert( mpz_sizeinbase( message, 2 ) <= mpz_sizeinbase( curve->n, 2 ) );

        point Q = point_init();

        // Initializing variables
        mpz_t k, x, r, t1, t2, t3, t4, t5, s, n_div_2, rem, neg, seed;
        mpz_init( k );
        mpz_init( x );
        mpz_init( r );
        mpz_init( t1 );
        mpz_init( t2 );
        mpz_init( t3 );
        mpz_init( s );
        mpz_init( t4 );
        mpz_init( t5 );
        mpz_init( n_div_2 );
        mpz_init( rem );
        mpz_init( neg );
        mpz_init( seed );

        unsigned char* rand_char = ( unsigned char* ) calloc( 32, 1 );

        CHECK_STATE( urandom );
        urandom.read( reinterpret_cast< char* >( rand_char ), 32 );  // Read from urandom
        CHECK_STATE( urandom );

    signature_sign_start:


        CHECK_STATE( urandom );
        urandom.read( reinterpret_cast< char* >( rand_char ), 32 );  // Read from urandom
        CHECK_STATE( urandom );

        mpz_import( seed, 32, 1, sizeof( rand_char[0] ), 0, 0, rand_char );

        mpz_mod( k, seed, curve->p );

        // mpz_set_str(k, "49a0d7b786ec9cde0d0721d72804befd06571c974b191efb42ecf322ba9ddd9a", 16);
        //  mpz_set_str(k, "DC87789C4C1A09C97FF4DE72C0D0351F261F10A2B9009C80AEE70DDEC77201A0", 16);
        // mpz_set_str(k,"29932781130098090011281004827843485745127563886526054275935615017309884975795",10);

        // Calculate x
        point_multiplication( Q, k, curve->G, curve );
        mpz_set( x, Q->x );

        // Calculate r
        mpz_mod( r, x, curve->n );
        if ( !mpz_sgn( r ) )  // Start over if r=0, note haven't been tested memory might die :)
            goto signature_sign_start;


        // Calculate s
        // s = k¯¹(e+d*r) mod n = (k¯¹ mod n) * ((e+d*r) mod n) mod n
        // number_theory_inverse(t1, k, curve->n);//t1 = k¯¹ mod n
        mpz_invert( t1, k, curve->n );
        mpz_mul( t2, private_key, r );  // t2 = d*r
        mpz_add( t3, message, t2 );     // t3 = e+t2
        mpz_mod( t4, t3, curve->n );    // t2 = t3 mod n
        mpz_mul( t5, t4, t1 );          // t3 = t2 * t1
        mpz_mod( s, t5, curve->n );     // s = t3 mod n

        // Calculate v

        mpz_mod_ui( rem, Q->y, 2 );
        mpz_t s_mul_2;
        mpz_init( s_mul_2 );
        mpz_mul_ui( s_mul_2, s, 2 );

        unsigned b = 0;
        if ( mpz_cmp( s_mul_2, curve->n ) > 0 ) {
            b = 1;
        }
        sig->v = mpz_get_ui( rem ) ^ b;

        mpz_cdiv_q_ui( n_div_2, curve->n, 2 );

        if ( mpz_cmp( s, n_div_2 ) > 0 ) {
            mpz_sub( neg, curve->n, s );
            mpz_set( s, neg );
        }

        // Set signature
        mpz_set( sig->r, r );
        mpz_set( sig->s, s );

        free( rand_char );
        point_clear( Q );

        mpz_clear( k );
        mpz_clear( r );
        mpz_clear( s );
        mpz_clear( x );
        mpz_clear( rem );
        mpz_clear( neg );
        mpz_clear( t1 );
        mpz_clear( t2 );
        mpz_clear( t3 );
        mpz_clear( seed );
        mpz_clear( n_div_2 );
        mpz_clear( s_mul_2 );
    }
}


tuple< ptr< string >, ptr< string >, ptr< string > > CryptoManager::sessionSignECDSAInternal(
    ptr< SHAHash > _hash, block_id _blockID ) {
    CHECK_ARGUMENT( _hash );


    if ( !ecdsaCurve ) {
        ecdsaCurve = domain_parameters_init();
        domain_parameters_load_curve( ecdsaCurve, secp256k1 );
    }


    ptr< MPZNumber > privateKey = nullptr;
    ptr< string > publicKey = nullptr;
    ptr< string > pkSig = nullptr;


    {
        LOCK( sessionKeysLock );

        if ( sessionKeys.exists( ( uint64_t ) _blockID ) ) {
            tie( privateKey, publicKey, pkSig ) = sessionKeys.get( ( uint64_t ) _blockID );
            CHECK_STATE( privateKey );
            CHECK_STATE( publicKey );
            CHECK_STATE( pkSig );

        } else {
            tie( privateKey, publicKey ) = localGenerateEcdsaKey();

            ptr< SHAHash > pKeyHash = nullptr;

            pKeyHash = calculatePublicKeyHash( publicKey, _blockID );

            CHECK_STATE( sgxECDSAKeyName );
            pkSig = sgxSignECDSA( pKeyHash, *sgxECDSAKeyName );
            CHECK_STATE( pkSig );

            sessionKeys.put( ( uint64_t ) _blockID, { privateKey, publicKey, pkSig } );
        }
    }

    mpz_t msgMpz;
    mpz_init( msgMpz );

    if ( mpz_set_str( msgMpz, _hash->toHex()->c_str(), 16 ) == -1 ) {
        mpz_clear( msgMpz );
        BOOST_THROW_EXCEPTION( InvalidStateException( "Can mpz_set_str hash", __CLASS_NAME__ ) );
    };

    signature sign = signature_init();

    signature_sign( sign, msgMpz, privateKey->number, ecdsaCurve );

    char arrR[mpz_sizeinbase( sign->r, 16 ) + 2];
    mpz_get_str( arrR, 16, sign->r );


    char arrS[mpz_sizeinbase( sign->s, 16 ) + 2];
    mpz_get_str( arrS, 16, sign->s );

    string r( arrR );
    string s( arrS );
    string v = to_string( sign->v );

    auto ret = make_shared< string >( v + ":" + r.substr( 2 ) + ":" + s.substr( 2 ) );

    mpz_clear( msgMpz );
    signature_free( sign );

    CHECK_STATE( ret );
    CHECK_STATE( publicKey );
    CHECK_STATE( pkSig );

    return { ret, publicKey, pkSig };
}

ptr< SHAHash > CryptoManager::calculatePublicKeyHash( ptr< string > publicKey, block_id _blockID ) {
    auto bytesToHash = make_shared< vector< uint8_t > >();

    auto bId = ( uint64_t ) _blockID;
    auto bidP = ( uint8_t* ) &bId;

    for ( uint64_t i = 0; i < sizeof( uint64_t ); i++ ) {
        bytesToHash->push_back( bidP[i] );
    }

    for ( uint64_t i = 0; i < publicKey->size(); i++ ) {
        bytesToHash->push_back( publicKey->at( i ) );
    }

    return SHAHash::calculateHash( bytesToHash );
}


ptr< string > CryptoManager::sgxSignECDSA( ptr< SHAHash > _hash, string& _keyName ) {
    CHECK_ARGUMENT( _hash );

    Json::Value result;
    {
        LOCK( clientLock );
        result = getSgxClient()->ecdsaSignMessageHash( 16, _keyName, *_hash->toHex() );
    }
    auto status = result["status"].asInt64();
    CHECK_STATE( status == 0 );
    string r = result["signature_r"].asString();
    string v = result["signature_v"].asString();
    string s = result["signature_s"].asString();

    auto ret = make_shared< string >( v + ":" + r.substr( 2 ) + ":" + s.substr( 2 ) );
    return ret;
}


bool CryptoManager::signECDSASigRSOpenSSL( const char* _hash ) {


    CHECK_ARGUMENT( _hash );

    auto ecKey = OpenSSLECDSAKey::generateKey();

    auto pubKeyStr = ecKey->getPublicKey();

    auto pubKey = make_shared<OpenSSLECDSAKey>(pubKeyStr);

    CHECK_STATE(pubKey);

    auto signature = ecKey->signHash(_hash);

    return pubKey->verifyHash(signature, _hash);

}


bool CryptoManager::verifyECDSASigRSOpenSSL(
    string& pubKeyStr, const char* hashHex, const char* signatureR, const char* signatureS ) {
    CHECK_STATE( pubKeyStr.size() == 128 )
    CHECK_ARGUMENT( hashHex );
    CHECK_ARGUMENT( signatureR );
    CHECK_ARGUMENT( signatureS );

    // bool result = false;

    int function_status = -1;
    EC_KEY* eckey = EC_KEY_new();
    if ( NULL == eckey ) {
        printf( "Failed to create new EC Key\n" );
        function_status = -1;
    } else {
        EC_GROUP* ecgroup = EC_GROUP_new_by_curve_name( NID_secp192k1 );
        if ( NULL == ecgroup ) {
            printf( "Failed to create new EC Group\n" );
            function_status = -1;
        } else {
            int set_group_status = EC_KEY_set_group( eckey, ecgroup );
            const int set_group_success = 1;
            if ( set_group_success != set_group_status ) {
                printf( "Failed to set group for EC Key\n" );
                function_status = -1;
            } else {
                const int gen_success = 1;
                int gen_status = EC_KEY_generate_key( eckey );
                if ( gen_success != gen_status ) {
                    printf( "Failed to generate EC Key\n" );
                    function_status = -1;
                } else {
                    ECDSA_SIG* signature =
                        ECDSA_do_sign( ( const unsigned char* ) hashHex, 32, eckey );
                    if ( NULL == signature ) {
                        printf( "Failed to generate EC Signature\n" );
                        function_status = -1;
                    } else {
                        int verify_status = ECDSA_do_verify(
                            ( const unsigned char* ) hashHex, 32, signature, eckey );
                        const int verify_success = 1;
                        if ( verify_success != verify_status ) {
                            printf( "Failed to verify EC Signature\n" );
                            function_status = -1;
                        } else {
                            printf( "Verifed EC Signature\n" );
                            function_status = 1;
                        }
                    }
                }
            }
            EC_GROUP_free( ecgroup );
        }
        EC_KEY_free( eckey );
    }

    return function_status == 1;
}


bool CryptoManager::verifyECDSASigRS( string& pubKeyStr, const char* hashHex,
    const char* signatureR, const char* signatureS, int base ) {
    CHECK_ARGUMENT( hashHex );
    CHECK_ARGUMENT( signatureR );
    CHECK_ARGUMENT( signatureS );

    bool result = false;

    signature sig = signature_init();

    auto x = pubKeyStr.substr( 0, 64 );
    auto y = pubKeyStr.substr( 64, 128 );
    domain_parameters curve = domain_parameters_init();
    domain_parameters_load_curve( curve, secp256k1 );
    point publicKey = point_init();

    mpz_t msgMpz;
    mpz_init( msgMpz );
    if ( mpz_set_str( msgMpz, hashHex, 16 ) == -1 ) {
        LOG( err, "invalid message hash" + string( hashHex ) );
        goto clean;
    }

    if ( signature_set_str( sig, signatureR, signatureS, base ) != 0 ) {
        LOG( err, "Failed to set str signature" );
        goto clean;
    }

    point_set_hex( publicKey, x.c_str(), y.c_str() );

    if ( !signature_verify( msgMpz, sig, publicKey, curve ) ) {
        LOG( err, "signature_verify failed " );
        goto clean;
    }

    result = true;

clean:

    mpz_clear( msgMpz );
    domain_parameters_clear( curve );
    point_clear( publicKey );
    signature_free( sig );

    return result;
}


bool CryptoManager::localVerifyECDSAInternal(
    ptr< SHAHash > _hash, ptr< string > _sig, ptr< string > _publicKey ) {
    CHECK_ARGUMENT( _hash );
    CHECK_ARGUMENT( _sig );
    CHECK_ARGUMENT( _publicKey );

    signature sig = NULL;

    try {
        sig = signature_init();
        CHECK_STATE( sig );

        auto firstColumn = _sig->find( ":" );

        if ( firstColumn == string::npos || firstColumn == _sig->length() - 1 ) {
            LOG( err, "Misfomatted signature" );
            return false;
        }

        auto secondColumn = _sig->find( ":", firstColumn + 1 );


        if ( secondColumn == string::npos || secondColumn == _sig->length() - 1 ) {
            LOG( err, "Misformatted signature" );
            return false;
        }

        auto r = _sig->substr( firstColumn + 1, secondColumn - firstColumn - 1 );
        auto s = _sig->substr( secondColumn + 1, _sig->length() - secondColumn - 1 );

        if ( r == s ) {
            LOG( err, "r == s " );
            return false;
        }


        CHECK_STATE( firstColumn != secondColumn );

        if ( _publicKey->size() != 128 ) {
            LOG( err, "ECDSA verify fail: _publicKey->size() != 128" );
            return false;
        }

        signECDSASigRSOpenSSL((const char*) _hash->data());
        return verifyECDSASigRS( *_publicKey, _hash->toHex()->data(), r.data(), s.data(), 16 );
    } catch ( exception& e ) {
        LOG( err, "ECDSA sig did not verify: exception" + string( e.what() ) );
        return false;
    }
}


ptr< string > CryptoManager::signECDSA( ptr< SHAHash > _hash ) {
    CHECK_ARGUMENT( _hash );

    if ( isSGXEnabled ) {
        CHECK_STATE( sgxECDSAKeyName )
        auto result = sgxSignECDSA( _hash, *sgxECDSAKeyName );
        // CHECK_STATE( verifyECDSA( _hash, result, getSchain()->getNode()->getNodeID() ) );
        return result;
    } else {
        return _hash->toHex();
    }
}


tuple< ptr< string >, ptr< string >, ptr< string > > CryptoManager::sessionSignECDSA(
    ptr< SHAHash > _hash, block_id _blockId ) {
    CHECK_ARGUMENT( _hash );
    if ( isSGXEnabled ) {
        ptr< string > signature = nullptr;
        ptr< string > pubKey = nullptr;
        ptr< string > pkSig = nullptr;

        tie( signature, pubKey, pkSig ) = sessionSignECDSAInternal( _hash, _blockId );
        CHECK_STATE( signature );
        CHECK_STATE( pubKey );
        CHECK_STATE( pkSig );

        CHECK_STATE(
            sessionVerifyECDSA( _hash, signature, pubKey, getSchain()->getNode()->getNodeID() ) );
        return { signature, pubKey, pkSig };
    } else {
        return { _hash->toHex(), make_shared< string >( "" ), make_shared< string >( "" ) };
    }
}


bool CryptoManager::sessionVerifyECDSA(
    ptr< SHAHash > _hash, ptr< string > _sig, ptr< string > _publicKey, node_id _nodeId ) {
    CHECK_ARGUMENT( _hash )
    CHECK_ARGUMENT( _sig )
    CHECK_ARGUMENT( _publicKey );

    if ( isSGXEnabled ) {
        auto pubKey = ecdsaPublicKeyMap.at( ( uint64_t ) _nodeId );
        CHECK_STATE( pubKey );
        bool result = true;
        // auto result = localVerifyECDSAInternal( _hash, _sig, _publicKey );
        return result;
    } else {
        // mockup - used for testing
        if ( _sig->find( ":" ) != string::npos ) {
            LOG( critical,
                "Misconfiguration: this node is in mockup signature mode,"
                "but other node sent a real signature " );
            ASSERT( false );
        }

        return *_sig == *( _hash->toHex() );
    }
}


bool CryptoManager::verifyECDSA( ptr< SHAHash > _hash, ptr< string > _sig, node_id _nodeId ) {
    CHECK_ARGUMENT( _hash )
    CHECK_ARGUMENT( _sig )

    if ( isSGXEnabled ) {
        auto pubKey = ecdsaPublicKeyMap.at( ( uint64_t ) _nodeId );
        CHECK_STATE( pubKey );
        auto result = localVerifyECDSAInternal( _hash, _sig, pubKey );
        return result;
    } else {
        // mockup - used for testing
        if ( _sig->find( ":" ) != string::npos ) {
            LOG( critical,
                "Misconfiguration: this node is in mockup signature mode,"
                "but other node sent a real signature " );
            ASSERT( false );
        }

        return *_sig == *( _hash->toHex() );
    }
}


ptr< ThresholdSigShare > CryptoManager::signDAProofSigShare( ptr< BlockProposal > _p ) {
    CHECK_ARGUMENT( _p );

    auto result = signSigShare( _p->getHash(), _p->getBlockID() );
    CHECK_STATE( result );
    return result;
}

ptr< ThresholdSigShare > CryptoManager::signBinaryConsensusSigShare(
    ptr< SHAHash > _hash, block_id _blockId ) {
    CHECK_ARGUMENT( _hash );
    auto result = signSigShare( _hash, _blockId );
    CHECK_STATE( result );
    return result;
}

ptr< ThresholdSigShare > CryptoManager::signBlockSigShare(
    ptr< SHAHash > _hash, block_id _blockId ) {
    CHECK_ARGUMENT( _hash );
    auto result = signSigShare( _hash, _blockId );
    CHECK_STATE( result );
    return result;
}

ptr< ThresholdSigShare > CryptoManager::signSigShare( ptr< SHAHash > _hash, block_id _blockId ) {
    CHECK_ARGUMENT( _hash );
    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    if ( getSchain()->getNode()->isSgxEnabled() ) {
        Json::Value jsonShare;

        {
            LOCK( clientLock );

            jsonShare = getSgxClient()->blsSignMessageHash( *getSgxBlsKeyName(), *_hash->toHex(),
                requiredSigners, totalSigners, ( uint64_t ) getSchain()->getSchainIndex() );
        }

        CHECK_STATE( jsonShare["status"] == 0 );

        ptr< string > sigShare = make_shared< string >( jsonShare["signatureShare"].asString() );

        auto sig = make_shared< BLSSigShare >(
            sigShare, ( uint64_t ) getSchain()->getSchainIndex(), requiredSigners, totalSigners );
        return make_shared< ConsensusBLSSigShare >( sig, sChain->getSchainID(), _blockId );

    } else {
        auto sigShare = _hash->toHex();
        return make_shared< MockupSigShare >( sigShare, sChain->getSchainID(), _blockId,
            sChain->getSchainIndex(), sChain->getTotalSigners(), sChain->getRequiredSigners() );
    }
}

ptr< ThresholdSigShareSet > CryptoManager::createSigShareSet( block_id _blockId ) {
    if ( getSchain()->getNode()->isSgxEnabled() ) {
        return make_shared< ConsensusSigShareSet >( _blockId, totalSigners, requiredSigners );
    } else {
        return make_shared< MockupSigShareSet >( _blockId, totalSigners, requiredSigners );
    }
}


ptr< ThresholdSigShare > CryptoManager::createSigShare(
    ptr< string > _sigShare, schain_id _schainID, block_id _blockID, schain_index _signerIndex ) {
    CHECK_ARGUMENT( _sigShare );
    CHECK_STATE( totalSigners >= requiredSigners );

    if ( getSchain()->getNode()->isSgxEnabled() ) {
        return make_shared< ConsensusBLSSigShare >(
            _sigShare, _schainID, _blockID, _signerIndex, totalSigners, requiredSigners );
    } else {
        return make_shared< MockupSigShare >(
            _sigShare, _schainID, _blockID, _signerIndex, totalSigners, requiredSigners );
    }
}

void CryptoManager::signProposalECDSA( BlockProposal* _proposal ) {
    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    CHECK_ARGUMENT( _proposal );
    auto signature = signECDSA( _proposal->getHash() );
    CHECK_STATE( signature );
    _proposal->addSignature( signature );
}

tuple< ptr< string >, ptr< string >, ptr< string > > CryptoManager::signNetworkMsg(
    NetworkMessage& _msg ) {
    MONITOR( __CLASS_NAME__, __FUNCTION__ );
    auto&& [signature, publicKey, pkSig] = sessionSignECDSA( _msg.getHash(), _msg.getBlockId() );
    CHECK_STATE( signature );
    CHECK_STATE( publicKey );
    CHECK_STATE( pkSig );

    return { signature, publicKey, pkSig };
}

bool CryptoManager::verifyNetworkMsg( NetworkMessage& _msg ) {
    MONITOR( __CLASS_NAME__, __FUNCTION__ );
    auto sig = _msg.getECDSASig();
    auto hash = _msg.getHash();
    auto publicKey = _msg.getPublicKey();
    auto pkSig = _msg.getPkSig();

    CHECK_STATE( sig );
    CHECK_STATE( publicKey );
    CHECK_STATE( pkSig );
    CHECK_STATE( hash );

    auto pkeyHash = calculatePublicKeyHash( publicKey, _msg.getBlockID() );

    if ( !verifyECDSA( pkeyHash, pkSig, _msg.getSrcNodeID() ) ) {
        LOG( warn, "PubKey ECDSA sig did not verify" );
        return false;
    }

    if ( !sessionVerifyECDSA( hash, sig, publicKey, _msg.getSrcNodeID() ) ) {
        LOG( warn, "ECDSA sig did not verify" );
        return false;
    }

    return true;
}

bool CryptoManager::verifyProposalECDSA(
    ptr< BlockProposal > _proposal, ptr< string > _hashStr, ptr< string > _signature ) {
    CHECK_ARGUMENT( _proposal );
    CHECK_ARGUMENT( _hashStr )
    CHECK_ARGUMENT( _signature )

    auto hash = _proposal->getHash();

    CHECK_STATE( hash );

    if ( *hash->toHex() != *_hashStr ) {
        LOG( warn, "Incorrect proposal hash" );
        return false;
    }

    if ( !verifyECDSA( hash, _signature, _proposal->getProposerNodeID() ) ) {
        LOG( warn, "ECDSA sig did not verify" );
        return false;
    }
    return true;
}

ptr< ThresholdSignature > CryptoManager::verifyThresholdSig(
    ptr< SHAHash > _hash, ptr< string > _signature, block_id _blockId ) {
    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    CHECK_ARGUMENT( _hash );
    CHECK_ARGUMENT( _signature );

    if ( getSchain()->getNode()->isSgxEnabled() ) {
        auto sig = make_shared< ConsensusBLSSignature >(
            _signature, _blockId, totalSigners, requiredSigners );

        CHECK_STATE( blsPublicKeyObj );

        if ( !blsPublicKeyObj->VerifySig(
                 _hash->getHash(), sig->getBlsSig(), requiredSigners, totalSigners ) ) {
            BOOST_THROW_EXCEPTION(
                InvalidStateException( "BLS Signature did not verify", __CLASS_NAME__ ) );
        }

        return sig;

    } else {
        auto sig =
            make_shared< MockupSignature >( _signature, _blockId, requiredSigners, totalSigners );

        if ( *sig->toString() != *_hash->toHex() ) {
            BOOST_THROW_EXCEPTION( InvalidArgumentException(
                "Mockup threshold signature did not verify", __CLASS_NAME__ ) );
        }
        return sig;
    }
}

using namespace CryptoPP;

ptr< void > CryptoManager::decodeSGXPublicKey( ptr< string > _keyHex ) {
    CHECK_ARGUMENT( _keyHex );

    HexDecoder decoder;
    CHECK_STATE( decoder.Put( ( unsigned char* ) _keyHex->data(), _keyHex->size() ) == 0 );
    decoder.MessageEnd();
    CryptoPP::ECP::Point q;
    size_t len = decoder.MaxRetrievable();
    q.identity = false;
    q.x.Decode( decoder, len / 2 );
    q.y.Decode( decoder, len / 2 );

    auto publicKey = make_shared< ECDSA< CryptoPP::ECP, CryptoPP::SHA256 >::PublicKey >();
    publicKey->Initialize( ASN1::secp256r1(), q );
    return publicKey;
}


ptr< string > CryptoManager::getSGXEcdsaPublicKey( ptr< string > _keyName, ptr< StubClient > _c ) {
    CHECK_ARGUMENT( _keyName );
    CHECK_ARGUMENT( _c );

    LOG( info, "Getting ECDSA public key for " + *_keyName );

    auto result = _c->getPublicECDSAKey( *_keyName );
    auto publicKey = make_shared< string >( result["publicKey"].asString() );

    LOG( info, "Got ECDSA public key: " + *publicKey );

    return publicKey;
}

pair< ptr< string >, ptr< string > > CryptoManager::generateSGXECDSAKey( ptr< StubClient > _c ) {
    CHECK_ARGUMENT( _c );

    auto result = _c->generateECDSAKey();
    auto status = result["status"].asInt64();
    CHECK_STATE( status == 0 );

    auto keyName = make_shared< string >( result["keyName"].asString() );
    auto publicKey = make_shared< string >( result["publicKey"].asString() );

    CHECK_STATE( keyName->size() > 10 );
    CHECK_STATE( publicKey->size() > 10 );
    CHECK_STATE( keyName->find( "NEK" ) != string::npos );


    auto publicKey2 = getSGXEcdsaPublicKey( keyName, _c );

    CHECK_STATE( publicKey2 );

    return { keyName, publicKey };
}


void CryptoManager::generateSSLClientCertAndKey( string& _fullPathToDir ) {
    const std::string VALID_CHARS =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    std::random_device random_device;
    std::mt19937 generator( random_device() );
    std::uniform_int_distribution< int > distribution( 0, VALID_CHARS.size() - 1 );
    std::string random_string;
    std::generate_n( std::back_inserter( random_string ), 10,
        [&]() { return VALID_CHARS[distribution( generator )]; } );
    system(
        ( "/usr/bin/openssl req -new -sha256 -nodes -out " + _fullPathToDir +
            "/csr  -newkey rsa:2048 -keyout " + _fullPathToDir + "/key -subj /CN=" + random_string )
            .data() );
    string str, csr;
    ifstream file;
    file.open( _fullPathToDir + "/csr" );
    while ( getline( file, str ) ) {
        csr.append( str );
        csr.append( "\n" );
    }

    jsonrpc::HttpClient client( "http://localhost:1027" );
    StubClient c( client, jsonrpc::JSONRPC_CLIENT_V2 );

    auto result = c.SignCertificate( csr );
    int64_t status = result["status"].asInt64();
    CHECK_STATE( status == 0 );
    string certHash = result["hash"].asString();
    result = c.GetCertificate( certHash );
    status = result["status"].asInt64();
    CHECK_STATE( status == 0 );
    string signedCert = result["cert"].asString();
    ofstream outFile;
    outFile.open( _fullPathToDir + "/cert" );
    outFile << signedCert;
}

ptr< StubClient > CryptoManager::getSgxClient() const {
    CHECK_STATE( sgxClient );
    return sgxClient;
}
ptr< BLSPublicKey > CryptoManager::getBlsPublicKeyObj() const {
    CHECK_STATE( blsPublicKeyObj );
    return blsPublicKeyObj;
}
