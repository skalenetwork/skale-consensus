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

#include "thirdparty/lrucache.hpp"
#include "thirdparty/lru_ordered_cache.hpp"

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


class  OpenSSLECDSAKey;
class  OpenSSLEdDSAKey;

class CryptoManager {



//    cache::lru_cache<uint64_t, tuple<ptr<MPZNumber>, string, string>> sessionKeys;

    cache::lru_cache<uint64_t, tuple<ptr<OpenSSLEdDSAKey>, string, string>> sessionKeys;
    cache::lru_ordered_cache<string, string> sessionPublicKeys;


    recursive_mutex sessionKeysLock;
    recursive_mutex publicSessionKeysLock;

    map<uint64_t , ptr< jsonrpc::HttpClient >> httpClients;
    map<uint64_t , ptr< StubClient >> sgxClients;


    ptr< StubClient > getSgxClient();


    uint64_t totalSigners;
    uint64_t requiredSigners;

    bool isSGXEnabled = false;
    bool isHTTPSEnabled = true;

    string sgxURL;
    string sgxSSLKeyFileFullPath;
    string sgxSSLCertFileFullPath;
    string sgxECDSAKeyName;
    ptr< vector<string> > sgxECDSAPublicKeys;
    string sgxBlsKeyName;
    ptr< vector< ptr< vector<string>>>> sgxBLSPublicKeys;
    ptr< BLSPublicKey > sgxBLSPublicKey;


    map<uint64_t , string> ecdsaPublicKeyMap;
    map<uint64_t , ptr<vector<string>>> blsPublicKeyMap;

    tuple< ptr< OpenSSLEdDSAKey >, string > localGenerateFastKey();

    ptr< BLSPublicKey > blsPublicKeyObj = nullptr;

    Schain* sChain = nullptr;


    string sign(const ptr< SHAHash >& _hash );

    tuple<string, string, string> sessionSign(const ptr< SHAHash >& _hash, block_id _blockId) ;

    bool sessionVerifySig(const ptr< SHAHash >& _hash, const string& _sig, const string& _publicKey );

    bool verifySig(const ptr< SHAHash >& _hash, const string& _sig, node_id _nodeId );

    ptr< ThresholdSigShare > signSigShare(const ptr< SHAHash >& _hash, block_id _blockId );

    void initSGXClient();

    static uint64_t parseSGXPort(const string& _url );

public:
    // This constructor is used for testing
    CryptoManager( uint64_t _totalSigners, uint64_t _requiredSigners, bool _isSGXEnabled,
        string _sgxURL = "", string _sgxSslKeyFileFullPath = "",
        string _sgxSslCertFileFullPath = "", string _sgxEcdsaKeyName = "",
        ptr< vector<string> > _sgxEcdsaPublicKeys = nullptr );

    explicit CryptoManager( Schain& sChain );

    Schain* getSchain() const;

    ptr< ThresholdSignature > verifyThresholdSig(
        const ptr< SHAHash >& _hash, const string& _signature, block_id _blockId );

    ptr< ThresholdSigShareSet > createSigShareSet( block_id _blockId );

    ptr< ThresholdSigShare > createSigShare(const string& _sigShare, schain_id _schainID,
        block_id _blockID, schain_index _signerIndex );

    void signProposal( BlockProposal* _proposal );

    bool verifyProposalECDSA(
        const ptr< BlockProposal >& _proposal, const string& _hashStr, const string& _signature );

    tuple<ptr<ThresholdSigShare>, string, string, string>  signDAProof(const ptr< BlockProposal >& _p );

    ptr< ThresholdSigShare > signBinaryConsensusSigShare(const ptr< SHAHash >& _hash, block_id _blockId );

    ptr< ThresholdSigShare > signBlockSigShare(const ptr< SHAHash >& _hash, block_id _blockId );

    tuple<string, string, string> signNetworkMsg( NetworkMessage& _msg );

    bool verifyNetworkMsg( NetworkMessage& _msg );

    static ptr< void > decodeSGXPublicKey(const string& _keyHex );

    static pair< string, string > generateSGXECDSAKey(const ptr< StubClient >& _c );

    static string getSGXEcdsaPublicKey(const string& _keyName, const  ptr< StubClient >& _c );

    static void generateSSLClientCertAndKey( string& _fullPathToDir );
    static void setSGXKeyAndCert( string& _keyFullPath, string& _certFullPath, uint64_t _sgxPort );


    string sgxSignECDSA(const ptr< SHAHash >& _hash, string& _keyName );

    tuple<string, string, string> sessionSignECDSA(const ptr< SHAHash >& _hash, block_id _blockID );

    bool verifyECDSA(const ptr< SHAHash >& _hash, const string& _sig, const string& _publicKey );


    bool verifyECDSASigRS( string& pubKeyStr, const char* hashHex,
                           const char* signatureR, const char* signatureS, int base);


    ptr< BLSPublicKey > getSgxBlsPublicKey();
    string getSgxBlsKeyName();

    static ptr< SHAHash > calculatePublicKeyHash(
        string publicKey, block_id _blockID);
};

#define RETRY_BEGIN while (true) { try {
#define RETRY_END   ; break ;} catch ( exception& e ) { \
  if ( e.what() && ( string( e.what() ).find( "Could not connect" ) != string::npos || string( e.what() ).find( "timed out" ) != string::npos ) ) { \
  LOG( err, "Could not connext to sgx server, retrying ... \n" + string( e.what() ) ); \
  sleep( 60 ); \
  } else { throw; } } }

#endif  // SKALED_CRYPTOMANAGER_H

