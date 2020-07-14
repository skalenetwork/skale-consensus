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

#ifndef SKALED_CRYPTOMANAGER_H
#define SKALED_CRYPTOMANAGER_H


#include "messages/NetworkMessage.h"
#include "openssl/ec.h"


#define USER_SPACE 1

#include "sgxwallet/secure_enclave/DomainParameters.h"
#include "sgxwallet/secure_enclave/Curves.h"
#include "sgxwallet/secure_enclave/NumberTheory.h"
#include "sgxwallet/secure_enclave/Point.h"
#include "sgxwallet/secure_enclave/Signature.h"

#include "thirdparty/lrucache.hpp"

class Schain;
class SHAHash;
class ConsensusBLSSigShare;
class ThresholdSigShareSet;
class ThresholdSigShare;
class BlockProposal;
class ThresholdSignature;
class StubClient;
class ECP;
class BLSPublicKey;

namespace CryptoPP {
class ECP;
template < class EC, class H >
struct ECDSA;
}  // namespace CryptoPP

class ECDSAVerify;

namespace jsonrpc {
class HttpClient;
}

class MPZNumber {

public:

    MPZNumber();
    ~MPZNumber();
    mpz_t number;
};



class CryptoManager {



    cache::lru_cache<uint64_t, pair<ptr<MPZNumber>, ptr<string>>> sessionKeys;

    recursive_mutex clientLock;
    recursive_mutex sessionKeysLock;

    ptr< StubClient > sgxClient = nullptr;


    ptr< StubClient > getSgxClient() const;


    ptr< jsonrpc::HttpClient > httpClient = nullptr;

    uint64_t totalSigners;
    uint64_t requiredSigners;

    bool isSGXEnabled = false;
    bool isHTTPSEnabled = true;

    ptr< string > sgxURL;
    ptr< string > sgxSSLKeyFileFullPath;
    ptr< string > sgxSSLCertFileFullPath;
    ptr< string > sgxECDSAKeyName;
    ptr< vector< string > > sgxECDSAPublicKeys;
    ptr< string > sgxBlsKeyName;
    ptr< vector< ptr< vector< string > > > > sgxBLSPublicKeys;
    ptr< BLSPublicKey > sgxBLSPublicKey;


    map<node_id, ptr<string>> ecdsaPublicKeyMap;
    map<node_id, ptr<vector<string>>> blsPublicKeyMap;

    std::tuple<ptr<MPZNumber> , ptr<string>> localGenerateEcdsaKey();

    ptr< BLSPublicKey > blsPublicKeyObj = nullptr;

    Schain* sChain = nullptr;

    void signature_sign(signature sig, mpz_t message, mpz_t private_key, domain_parameters curve);

    ptr< string > signECDSA( ptr< SHAHash > _hash );

    tuple<ptr< string >, ptr<string>> sessionSignECDSA( ptr< SHAHash > _hash, block_id _blockId) ;


    bool sessionVerifyECDSA( ptr< SHAHash > _hash, ptr< string > _sig,
                           ptr<string> _publicKey, node_id _nodeId );

    bool verifyECDSA( ptr< SHAHash > _hash, ptr< string > _sig, node_id _nodeId );

    ptr< ThresholdSigShare > signSigShare( ptr< SHAHash > _hash, block_id _blockId );

    void initSGXClient();

    static uint64_t parseSGXPort( ptr< string > _url );

public:
    // This constructor is used for testing
    CryptoManager( uint64_t _totalSigners, uint64_t _requiredSigners, bool _isSGXEnabled,
        ptr< string > _sgxURL = nullptr, ptr< string > _sgxSslKeyFileFullPath = nullptr,
        ptr< string > _sgxSslCertFileFullPath = nullptr, ptr< string > _sgxEcdsaKeyName = nullptr,
        ptr< vector< string > > _sgxEcdsaPublicKeys = nullptr );

    CryptoManager( Schain& sChain );

    Schain* getSchain() const;

    ptr< ThresholdSignature > verifyThresholdSig(
        ptr< SHAHash > _hash, ptr< string > _signature, block_id _blockId );

    ptr< ThresholdSigShareSet > createSigShareSet( block_id _blockId );

    ptr< ThresholdSigShare > createSigShare( ptr< string > _sigShare, schain_id _schainID,
        block_id _blockID, schain_index _signerIndex );

    void signProposalECDSA( BlockProposal* _proposal );

    bool verifyProposalECDSA(
        ptr< BlockProposal > _proposal, ptr< string > _hashStr, ptr< string > _signature );

    ptr< ThresholdSigShare > signDAProofSigShare( ptr< BlockProposal > _p );

    ptr< ThresholdSigShare > signBinaryConsensusSigShare( ptr< SHAHash > _hash, block_id _blockId );

    ptr< ThresholdSigShare > signBlockSigShare( ptr< SHAHash > _hash, block_id _blockId );

    tuple<ptr< string >, ptr<string>> signNetworkMsg( NetworkMessage& _msg );

    bool verifyNetworkMsg( NetworkMessage& _msg );

    static ptr< void > decodeSGXPublicKey( ptr< string > _keyHex );

    static pair< ptr< string >, ptr< string > > generateSGXECDSAKey( ptr< StubClient > _c );

    static ptr< string > getSGXEcdsaPublicKey( ptr< string > _keyName, ptr< StubClient > _c );

    static void generateSSLClientCertAndKey( string& _fullPathToDir );
    static void setSGXKeyAndCert( string& _keyFullPath, string& _certFullPath, uint64_t _sgxPort );




    ptr< string > sgxSignECDSA( ptr< SHAHash > _hash, string& _keyName );

    tuple<ptr< string >, ptr<string>> sessionSignECDSAInternal( ptr< SHAHash > _hash, block_id _blockID );


    bool localVerifyECDSAInternal( ptr< SHAHash > _hash, ptr< string > _sig, ptr< string > _publicKey );

    bool verifyECDSASigRS( string& pubKeyStr, const char* hashHex, const char* signatureR,
                                        const char* signatureS, int base );


    ptr< BLSPublicKey > getSgxBlsPublicKey();
    ptr< string > getSgxBlsKeyName();


    ptr< BLSPublicKey > getBlsPublicKeyObj() const;
};


#endif  // SKALED_CRYPTOMANAGER_H
