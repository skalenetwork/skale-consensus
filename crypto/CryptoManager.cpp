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

#include "CryptoManager.h"


void CryptoManager::initSGX() {
    if ( isSGXEnabled ) {
        if ( isHTTPSEnabled ) {
            CHECK_STATE( sgxSSLKeyFileFullPath );
            CHECK_STATE( sgxSSLCertFileFullPath );
            setSGXKeyAndCert( *sgxSSLKeyFileFullPath, *sgxSSLCertFileFullPath );
        }


        httpClient = make_shared< jsonrpc::HttpClient >( *sgxURL );
        sgxClient = make_shared< StubClient >( *httpClient, jsonrpc::JSONRPC_CLIENT_V2 );
    }
}
ptr< vector< string > > CryptoManager::getSgxBlsPublicKey() {
    CHECK_STATE( sgxBLSPublicKey && sgxBLSPublicKey->size() == 4 );
    return sgxBLSPublicKey;
}

ptr< string > CryptoManager::getSgxBlsKeyName() {
    return sgxBlsKeyName;
}

CryptoManager::CryptoManager( uint64_t _totalSigners, uint64_t _requiredSigners, bool _isSGXEnabled,
    ptr< string > _sgxURL, ptr< string > _sgxSslKeyFileFullPath,
    ptr< string > _sgxSslCertFileFullPath, ptr< string > _sgxEcdsaKeyName,
    ptr< vector< string > > _sgxEcdsaPublicKeys ) {
    CHECK_ARGUMENT( _totalSigners > _requiredSigners );


    totalSigners = _totalSigners;
    requiredSigners = _requiredSigners;


    isSGXEnabled = _isSGXEnabled;

    if ( _isSGXEnabled ) {
        CHECK_ARGUMENT( _sgxURL );
        CHECK_ARGUMENT( _sgxSslKeyFileFullPath );
        CHECK_ARGUMENT( _sgxSslCertFileFullPath );
        CHECK_ARGUMENT( _sgxEcdsaKeyName );
        CHECK_ARGUMENT( _sgxEcdsaPublicKeys );


        sgxURL = _sgxURL;
        sgxSSLKeyFileFullPath = _sgxSslKeyFileFullPath;
        sgxSSLKeyFileFullPath = _sgxSslKeyFileFullPath;
        sgxSSLCertFileFullPath = _sgxSslCertFileFullPath;
        sgxECDSAKeyName = _sgxEcdsaKeyName;
        sgxECDSAPublicKeys = _sgxEcdsaPublicKeys;
        sgxURL = _sgxURL;
    }

    initSGX();
}


CryptoManager::CryptoManager( Schain& _sChain ) : sChain( &_sChain ) {
    CHECK_ARGUMENT( sChain != nullptr );


    totalSigners = getSchain()->getTotalSigners();
    requiredSigners = getSchain()->getRequiredSigners();

    CHECK_ARGUMENT( totalSigners > requiredSigners );

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


        for ( uint64_t i = 0; i < ( uint64_t ) getSchain()->getNodeCount(); i++ ) {
            auto nodeId = getSchain()->getNode()->getNodeInfoByIndex( i + 1 )->getNodeID();
            ecdsaPublicKeyMap.emplace(
                nodeId, make_shared< string >( sgxECDSAPublicKeys->at( i ) ) );
            blsPublicKeyMap.emplace( nodeId, sgxBLSPublicKeys->at( i ) );
        }

        isHTTPSEnabled = sgxURL->find( "https:/" ) != string::npos;

        totalSigners = sChain->getTotalSigners();
        requiredSigners = sChain->getRequiredSigners();

        if ( isHTTPSEnabled ) {
            CHECK_STATE( sgxSSLCertFileFullPath );
            CHECK_STATE( sgxSSLKeyFileFullPath );
        }


        initSGX();


        blsPublicKeyObj =
            make_shared< BLSPublicKey >( getSgxBlsPublicKey(), requiredSigners, totalSigners );
    }
}

void CryptoManager::setSGXKeyAndCert( string& _keyFullPath, string& _certFullPath ) {
    jsonrpc::HttpClient::setKeyFileFullPath( _keyFullPath );
    jsonrpc::HttpClient::setCertFileFullPath( _certFullPath );
    jsonrpc::HttpClient::setSslClientPort( SGX_SSL_PORT );
}

Schain* CryptoManager::getSchain() const {
    return sChain;
}


ptr< string > CryptoManager::sgxSignECDSA( ptr< SHAHash > _hash, string& _keyName ) {
    auto result = getSgxClient()->ecdsaSignMessageHash( 16, _keyName, *_hash->toHex() );
    auto status = result["status"].asInt64();
    CHECK_STATE( status == 0 );
    string r = result["signature_r"].asString();
    string v = result["signature_v"].asString();
    string s = result["signature_s"].asString();

    auto ret =  make_shared< string >( v + ":" + r.substr( 2 ) + ":" + s.substr( 2 ) );

    cerr << "Signed:"<< *ret;

    return ret;



}

bool CryptoManager::verifyECDSASigRS( string& pubKeyStr, const char* hashHex, const char* signatureR,
                                                          const char *signatureS, int base) {
    bool result = false;

    signature sig = signature_init();

    auto x = pubKeyStr.substr(0, 64);
    auto y = pubKeyStr.substr(64, 128);
    domain_parameters curve = domain_parameters_init();
    domain_parameters_load_curve(curve, secp256k1);
    point publicKey = point_init();

    mpz_t msgMpz;
    mpz_init(msgMpz);
    if (mpz_set_str(msgMpz, hashHex, 16) == -1) {
        spdlog::error("invalid message hash {}", hashHex);
        goto clean;
    }

    if (signature_set_str(sig, signatureR, signatureS, base) != 0) {
        spdlog::error("Failed to set str signature");
        goto clean;
    }

    point_set_hex(publicKey, x.c_str(), y.c_str());
    if (!signature_verify(msgMpz, sig, publicKey, curve)) {
        spdlog::error("ECDSA sig not verified");
        goto clean;
    }

    result = true;

    clean:

    mpz_clear(msgMpz);
    domain_parameters_clear(curve);
    point_clear(publicKey);
    signature_free(sig);

    return result;
}



bool CryptoManager::sgxVerifyECDSA(
    ptr< SHAHash > _hash, ptr< string > _publicKey, ptr< string > _sig ) {
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

        return verifyECDSASigRS( *_publicKey, _hash->toHex()->data(), r.data(), s.data(), 16 );
    } catch (exception& e) {
        LOG(err, "ECDSA Sig did not verify: exception" +  string(e.what()));
        return false;
    }
}


ptr< string > CryptoManager::signECDSA( ptr< SHAHash > _hash ) {
    CHECK_ARGUMENT( _hash );
    if ( isSGXEnabled ) {
        auto result = sgxSignECDSA( _hash, *sgxECDSAKeyName );
        CHECK_STATE( verifyECDSA( _hash, result, getSchain()->getNode()->getNodeID() ) );
        return result;
    } else {
        return _hash->toHex();
    }
}

bool CryptoManager::verifyECDSA( ptr< SHAHash > _hash, ptr< string > _sig, node_id _nodeId ) {
    CHECK_ARGUMENT( _hash != nullptr )
    CHECK_ARGUMENT( _sig != nullptr )

    cerr << "Verifying signature:" + *_hash->toHex() + ":" + *_sig + ":" + to_string( _nodeId )
         << endl;

    if ( isSGXEnabled ) {
        auto pubKey = ecdsaPublicKeyMap.at( _nodeId );
        CHECK_STATE( pubKey );
        auto result = sgxVerifyECDSA( _hash, pubKey, _sig );

        return result;
    } else {
        // mockup - used for testing
        return *_sig == *( _hash->toHex() );
    }
}


ptr< ThresholdSigShare > CryptoManager::signDAProofSigShare( ptr< BlockProposal > _p ) {
    CHECK_ARGUMENT( _p != nullptr );
    return signSigShare( _p->getHash(), _p->getBlockID() );
}

ptr< ThresholdSigShare > CryptoManager::signBinaryConsensusSigShare(
    ptr< SHAHash > _hash, block_id _blockId ) {
    return signSigShare( _hash, _blockId );
}

ptr< ThresholdSigShare > CryptoManager::signBlockSigShare(
    ptr< SHAHash > _hash, block_id _blockId ) {
    return signSigShare( _hash, _blockId );
}

ptr< ThresholdSigShare > CryptoManager::signSigShare( ptr< SHAHash > _hash, block_id _blockId ) {
    MONITOR( __CLASS_NAME__, __FUNCTION__ )

    if ( getSchain()->getNode()->isSgxEnabled() ) {
        auto jsonShare = getSgxClient()->blsSignMessageHash( *getSgxBlsKeyName(), *_hash->toHex(),
            requiredSigners, totalSigners, ( uint64_t ) getSchain()->getSchainIndex() );
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
    CHECK_STATE( totalSigners > requiredSigners );


    if ( getSchain()->getNode()->isSgxEnabled() ) {
        return make_shared< ConsensusBLSSigShare >(
            _sigShare, _schainID, _blockID, _signerIndex, totalSigners, requiredSigners );
    } else {
        return make_shared< MockupSigShare >(
            _sigShare, _schainID, _blockID, _signerIndex, totalSigners, requiredSigners );
    }
}

void CryptoManager::signProposalECDSA( BlockProposal* _proposal ) {
    CHECK_ARGUMENT( _proposal != nullptr );
    auto signature = signECDSA( _proposal->getHash() );
    CHECK_STATE( signature != nullptr );
    _proposal->addSignature( signature );
}

ptr< string > CryptoManager::signNetworkMsg( NetworkMessage& _msg ) {
    auto signature = signECDSA( _msg.getHash() );
    CHECK_STATE( signature != nullptr );
    return signature;
}

bool CryptoManager::verifyNetworkMsg( NetworkMessage& _msg ) {
    auto sig = _msg.getECDSASig();
    auto hash = _msg.getHash();

    if ( !verifyECDSA( hash, sig, _msg.getSrcNodeID() ) ) {
        LOG( warn, "ECDSA sig did not verify" );
        return false;
    }

    return true;
}

bool CryptoManager::verifyProposalECDSA(
    ptr< BlockProposal > _proposal, ptr< string > _hashStr, ptr< string > _signature ) {
    CHECK_ARGUMENT( _proposal != nullptr );
    CHECK_ARGUMENT( _hashStr != nullptr )
    CHECK_ARGUMENT( _signature != nullptr )
    auto hash = _proposal->getHash();


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


    if ( getSchain()->getNode()->isSgxEnabled() ) {
        auto hash = make_shared< std::array< uint8_t, 32 > >();

        memcpy( hash->data(), _hash->data(), 32 );


        auto sig = make_shared< ConsensusBLSSignature >(
            _signature, _blockId, requiredSigners, totalSigners );

        if ( !blsPublicKeyObj->VerifySig(
                 hash, sig->getBlsSig(), requiredSigners, totalSigners ) ) {
            BOOST_THROW_EXCEPTION(
                InvalidArgumentException( "BLS Signature did not verify", __CLASS_NAME__ ) );
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
    CHECK_STATE( _keyName );
    CHECK_ARGUMENT( _c );
    auto result = _c->getPublicECDSAKey( *_keyName );
    auto publicKey = make_shared< string >( result["publicKey"].asString() );
    return publicKey;
}

pair< ptr< string >, ptr< string > > CryptoManager::generateSGXECDSAKey( ptr< StubClient > _c ) {
    CHECK_ARGUMENT( _c );

    auto result = _c->generateECDSAKey();

    auto status = result["status"].asInt64();
    CHECK_STATE( status == 0 );
    cerr << result << endl;
    auto keyName = make_shared< string >( result["keyName"].asString() );
    auto publicKey = make_shared< string >( result["publicKey"].asString() );

    CHECK_STATE( keyName->size() > 10 );
    CHECK_STATE( publicKey->size() > 10 );
    CHECK_STATE( keyName->find( "NEK" ) != string::npos );


    auto publicKey2 = getSGXEcdsaPublicKey( keyName, _c );

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
