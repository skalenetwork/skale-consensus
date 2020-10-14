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
#include <openssl/obj_mac.h>  // for NID_secp256k1

#include <openssl/bn.h>


#include <sys/types.h>

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
    : sessionKeys( SESSION_KEY_CACHE_SIZE ), sessionPublicKeys( SESSION_PUBLIC_KEY_CACHE_SIZE ) {
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
    : sessionKeys( SESSION_KEY_CACHE_SIZE ),
      sessionPublicKeys( SESSION_PUBLIC_KEY_CACHE_SIZE ),
      sChain( &_sChain ) {
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
                auto publicKey = getSGXEcdsaPublicKey( sgxECDSAKeyName, getSgxClient() );
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

std::tuple< ptr< OpenSSLECDSAKey >, ptr< string > > CryptoManager::localGenerateEcdsaKey() {
    auto key = OpenSSLECDSAKey::generateKey();
    auto pKey = key->getPublicKey();

    return { key, pKey };
}


tuple< ptr< string >, ptr< string >, ptr< string > > CryptoManager::sessionSignECDSA(
    ptr< SHAHash > _hash, block_id _blockID ) {
    CHECK_ARGUMENT( _hash );


    if ( !ecdsaCurve ) {
        ecdsaCurve = domain_parameters_init();
        domain_parameters_load_curve( ecdsaCurve, secp256k1 );
    }


    ptr< OpenSSLECDSAKey > privateKey = nullptr;
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

    auto ret = privateKey->signHash( ( const char* ) _hash->data() );

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

    RETRY_BEGIN
    result = getSgxClient()->ecdsaSignMessageHash( 16, _keyName, *_hash->toHex() );
    RETRY_END


    auto status = JSONFactory::getInt64( result, "status" );
    CHECK_STATE( status == 0 );
    string r = JSONFactory::getString(result,"signature_r");
    string v = JSONFactory::getString(result, "signature_v");
    string s = JSONFactory::getString(result, "signature_s");

    auto ret = make_shared< string >( v + ":" + r.substr( 2 ) + ":" + s.substr( 2 ) );
    return ret;
}


bool CryptoManager::verifyECDSA(
    ptr< SHAHash > _hash, ptr< string > _sig, ptr< string > _publicKey ) {

    auto key = make_shared<OpenSSLECDSAKey>(_publicKey, true);

    CHECK_ARGUMENT( _publicKey );
    CHECK_ARGUMENT( _publicKey->size() != 128 );

    auto x = _publicKey->substr( 0, 64 );
    auto y = _publicKey->substr( 64, 128 );
    BIGNUM* xBN = BN_new();
    BIGNUM* yBN = BN_new();
    CHECK_STATE(BN_hex2bn(&xBN, x.c_str()) != 0);
    CHECK_STATE(BN_hex2bn(&yBN, y.c_str()) != 0);
    auto pubKey = EC_KEY_new_by_curve_name(NID_secp256k1);
    CHECK_STATE(EC_KEY_set_public_key_affine_coordinates(pubKey, xBN, yBN) == 1);


    CHECK_ARGUMENT( _hash );
    CHECK_ARGUMENT( _sig );



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

        CHECK_STATE(BN_hex2bn(&rBN, r.c_str()) != 0);
        CHECK_STATE(BN_hex2bn(&sBN, s.c_str()) != 0);


        auto oSig = ECDSA_SIG_new();

        CHECK_STATE(oSig);

        CHECK_STATE(ECDSA_SIG_set0(oSig, rBN, sBN) != 0);

        CHECK_STATE(ECDSA_do_verify( ( const unsigned char* )
                                         _hash->data(), 32, oSig, pubKey) == 1);

        returnValue = true;

    } catch ( exception& e ) {
        LOG( err, "ECDSA sig did not verify: exception" + string( e.what() ) );
        returnValue = false;

    }

    clean:

    return returnValue;
}


ptr< string > CryptoManager::sign( ptr< SHAHash > _hash ) {
    CHECK_ARGUMENT( _hash );

    if ( isSGXEnabled ) {
        CHECK_STATE( sgxECDSAKeyName )
        auto result = sgxSignECDSA( _hash, *sgxECDSAKeyName );
        return result;
    } else {
        return _hash->toHex();
    }
}


tuple< ptr< string >, ptr< string >, ptr< string > > CryptoManager::sessionSign(
    ptr< SHAHash > _hash, block_id _blockId ) {
    CHECK_ARGUMENT( _hash );
    if ( isSGXEnabled ) {
        ptr< string > signature = nullptr;
        ptr< string > pubKey = nullptr;
        ptr< string > pkSig = nullptr;

        tie( signature, pubKey, pkSig ) = sessionSignECDSA( _hash, _blockId );
        CHECK_STATE( signature );
        CHECK_STATE( pubKey );
        CHECK_STATE( pkSig );
        return { signature, pubKey, pkSig };
    } else {
        return { _hash->toHex(), make_shared< string >( "" ), make_shared< string >( "" ) };
    }
}


bool CryptoManager::sessionVerifySig(
    ptr< SHAHash > _hash, ptr< string > _sig, ptr< string > _publicKey ) {
    CHECK_ARGUMENT( _hash )
    CHECK_ARGUMENT( _sig )
    CHECK_ARGUMENT( _publicKey );

    if ( isSGXEnabled ) {
        auto pkey = make_shared< OpenSSLECDSAKey >( _publicKey, false );

        return pkey->verifyHash( _sig, ( const char* ) _hash->data() );

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


bool CryptoManager::verifySig( ptr< SHAHash > _hash, ptr< string > _sig, node_id _nodeId ) {
    CHECK_ARGUMENT( _hash )
    CHECK_ARGUMENT( _sig )

    if ( isSGXEnabled ) {
        if ( ecdsaPublicKeyMap.count( ( uint64_t ) _nodeId ) == 0 ) {
            // if there is no key report the signature as failed
            return false;
        }

        auto pubKey = ecdsaPublicKeyMap.at( ( uint64_t ) _nodeId );
        CHECK_STATE( pubKey );
        auto result = verifyECDSA( _hash, _sig, pubKey );

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


        RETRY_BEGIN
        jsonShare = getSgxClient()->blsSignMessageHash( *getSgxBlsKeyName(), *_hash->toHex(),
            requiredSigners, totalSigners );
        RETRY_END

        auto status = JSONFactory::getInt64( jsonShare, "status" );
        CHECK_STATE( status == 0 );

        ptr< string > sigShare = make_shared< string >(
            JSONFactory::getString(jsonShare, "signatureShare"));

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

void CryptoManager::signProposal( BlockProposal* _proposal ) {
    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    CHECK_ARGUMENT( _proposal );
    auto signature = sign( _proposal->getHash() );
    CHECK_STATE( signature );
    _proposal->addSignature( signature );
}

tuple< ptr< string >, ptr< string >, ptr< string > > CryptoManager::signNetworkMsg(
    NetworkMessage& _msg ) {
    MONITOR( __CLASS_NAME__, __FUNCTION__ );
    auto&& [signature, publicKey, pkSig] = sessionSign( _msg.getHash(), _msg.getBlockId() );
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


    {
        LOCK( publicSessionKeysLock )

        if ( sessionPublicKeys.exists( *pkSig ) ) {
            auto publicKey2 = sessionPublicKeys.get( *pkSig );
            return ( publicKey2 == *publicKey );
        }
    }

    auto pkeyHash = calculatePublicKeyHash( publicKey, _msg.getBlockID() );

    if ( isSGXEnabled ) {
        if ( !verifySig( pkeyHash, pkSig, _msg.getSrcNodeID() ) ) {
            LOG( warn, "PubKey ECDSA sig did not verify" );
            return false;
        }
    }

    if ( !sessionVerifySig( hash, sig, publicKey ) ) {
        LOG( warn, "ECDSA sig did not verify" );
        return false;
    }


    {
        LOCK( publicSessionKeysLock )
        if ( !sessionPublicKeys.exists( *pkSig ) )
            sessionPublicKeys.put( *pkSig, *publicKey );
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

    if ( !verifySig( hash, _signature, _proposal->getProposerNodeID() ) ) {
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

    Json::Value result;

    RETRY_BEGIN
    result = _c->getPublicECDSAKey( *_keyName );
    RETRY_END

    auto status = JSONFactory::getInt64(result, "status");
    CHECK_STATE( status == 0 );

    auto publicKey = make_shared< string >(
        JSONFactory::getString(result,"publicKey"));

    LOG( info, "Got ECDSA public key: " + *publicKey );

    return publicKey;
}

pair< ptr< string >, ptr< string > > CryptoManager::generateSGXECDSAKey( ptr< StubClient > _c ) {
    CHECK_ARGUMENT( _c );

    Json::Value result;
    RETRY_BEGIN
    result = _c->generateECDSAKey();
    RETRY_END
    auto status = JSONFactory::getInt64(result, "status");
    CHECK_STATE( status == 0 );

    auto keyName = make_shared< string >(
        JSONFactory::getString(result, "keyName"));
    auto publicKey = make_shared< string >(
        JSONFactory::getString(result,"publicKey"));

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

    Json::Value result;

    RETRY_BEGIN
    result = c.SignCertificate( csr );
    RETRY_END

    auto status = JSONFactory::getInt64(result, "status");
    CHECK_STATE( status == 0 );
    string certHash = JSONFactory::getString(result, "hash");

    RETRY_BEGIN
    result = c.GetCertificate( certHash );
    RETRY_END

    status = JSONFactory::getInt64(result,"status");
    CHECK_STATE( status == 0 );

    string signedCert = JSONFactory::getString(result, "cert");
    ofstream outFile;
    outFile.open( _fullPathToDir + "/cert" );
    outFile << signedCert;
}



ptr< StubClient > CryptoManager::getSgxClient() {
    auto tid = ( uint64_t ) pthread_self();

    if ( httpClients.count( tid ) == 0 ) {
        CHECK_STATE( sgxClients.count( tid ) == 0 );

        auto httpClient = make_shared< jsonrpc::HttpClient >( *sgxURL );

        httpClients.insert( { tid, httpClient } );
        sgxClients.insert(
            { tid, make_shared< StubClient >( *httpClient, jsonrpc::JSONRPC_CLIENT_V2 ) } );
    }

    return sgxClients.at( tid );
}
