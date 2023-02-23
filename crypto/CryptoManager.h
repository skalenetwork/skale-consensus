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

#include "sgxclient/SgxZmqClient.h"

#define USER_SPACE 1

#include "thirdparty/lru_ordered_cache.hpp"
#include "thirdparty/lrucache.hpp"

class Schain;

class BLAKE3Hash;

class ConsensusBLSSigShare;

class ThresholdSigShareSet;

class ThresholdSigShare;

class BlockProposal;

class ThresholdSignature;

class StubClient;

class ECP;

class BLSPublicKey;

class BLSSigShare;

namespace CryptoPP {
    class ECP;

    template<class EC, class H>
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


class OpenSSLECDSAKey;

class OpenSSLEdDSAKey;

class CryptoManager {

    static list<uint64_t> ecdsaSignTimes;
    static recursive_mutex ecdsaSignMutex;
    static atomic<uint64_t> ecdsaSignTotal;

    static list<uint64_t> blsSignTimes;
    static recursive_mutex blsSignMutex;
    static atomic<uint64_t> blsSignTotal;

    static atomic<uint64_t> blsCounter;
    static atomic<uint64_t> ecdsaCounter;


    cache::lru_cache<uint64_t, tuple<ptr<OpenSSLEdDSAKey>, string, string> >
            sessionKeys;                                               // tsafe
    cache::lru_ordered_cache<string, string> sessionPublicKeys;  // tsafe
    recursive_mutex sessionKeysLock;
    recursive_mutex publicSessionKeysLock;

    map<uint64_t, ptr<jsonrpc::HttpClient> > httpClients;  // tsafe
    map<uint64_t, ptr<StubClient> > sgxClients;            // tsafe


    ptr<SgxZmqClient> zmqClient = nullptr;

    recursive_mutex clientsLock;

    map<uint64_t, string> ecdsaPublicKeyMap;  // tsafe
    recursive_mutex ecdsaPublicKeyMapLock;
    recursive_mutex historicEcdsaPublicKeyMapLock;


    map<uint64_t, ptr<vector<string> > > blsPublicKeySharesMapByIndex;  // tsafe

    ptr<vector<ptr<vector<string> > > > sgxBLSPublicKeyShares;  // tsafe

    ptr<vector<string> > sgxECDSAPublicKeys;  // tsafe

    ptr<map<uint64_t, ptr<BLSPublicKey> > > previousBlsPublicKeys;

    ptr<map<uint64_t, string> > historicECDSAPublicKeys;

    ptr<map<uint64_t, vector<uint64_t>>> historicNodeGroups;

    uint64_t totalSigners;
    uint64_t requiredSigners;

    bool isSGXEnabled = false;
    bool isSyncNode = false;
    bool verifyRealSignatures = false;

    bool isHTTPSEnabled = true;
    bool isSSLCertEnabled = false;

    string sgxSSLKeyFileFullPath;
    string sgxSSLCertFileFullPath;
    string sgxECDSAKeyName;
    string sgxBlsKeyName;


    ptr<BLSPublicKey> sgxBLSPublicKey;

    Schain *sChain = nullptr;

    static bool retryHappened;

    static string sgxURL;

    string sgxDomainName;
    uint16_t sgxPort = 0;

    uint64_t simulateBLSSigFailBlock = 0;

    ptr<StubClient> getSgxClient();

    tuple<ptr<OpenSSLEdDSAKey>, string> localGenerateFastKey();

    string sign(BLAKE3Hash &_hash);

    tuple<string, string, string> signSession(
            BLAKE3Hash &_hash, block_id _blockId);

    void verifyECDSASig(BLAKE3Hash &_hash, const string &_sig, node_id _nodeId, uint64_t _timeStamp);

    ptr<ThresholdSigShare> signSigShare(
            BLAKE3Hash &_hash, block_id _blockId, bool _forceMockup);

    ptr<ThresholdSigShare> signDAProofSigShare(
            BLAKE3Hash &_hash, block_id _blockId, uint64_t _timestamp,  bool _forceMockup);


    void initSGXClient();

    static pair<string, uint64_t> parseSGXDomainAndPort(const string &_url);


public:

    static ifstream urandom;

    void verifyThresholdSig(
            ptr<ThresholdSignature> _signature, BLAKE3Hash &_hash, const TimeStamp &_ts = TimeStamp(uint64_t(-1), 0));

    void verifyBlockSig(string &_signature, block_id _blockId, BLAKE3Hash &_hash,
                        const TimeStamp &_ts = TimeStamp(uint64_t(-1), 0));

    void verifyThresholdSigShare(
            ptr<ThresholdSigShare> _sigShare, BLAKE3Hash &_hash);


    static bool isRetryHappened();

    static void setRetryHappened(bool retryHappened);

    void verifySessionEdDSASig(
            BLAKE3Hash &_hash, const string &_sig, const string &_publicKey);

    // This constructor is used for testing
    CryptoManager(uint64_t _totalSigners, uint64_t _requiredSigners, bool _isSGXEnabled,
                  string _sgxURL = "", string _sgxSslKeyFileFullPath = "",
                  string _sgxSslCertFileFullPath = "", string _sgxEcdsaKeyName = "",
                  ptr<vector<string> > _sgxEcdsaPublicKeys = nullptr);

    explicit CryptoManager(Schain &sChain);

    Schain *getSchain() const;


    void verifyDAProofSigShare(ptr<ThresholdSigShare> _sigShare, schain_index _schainIndex,
                               BLAKE3Hash &_hash, node_id _nodeId, bool _forceMockup);


    ptr<ThresholdSignature> verifyDAProofThresholdSig(
            BLAKE3Hash &_hash, const string &_signature, block_id _blockId, uint64_t _timestamp);

    void verifyProposalECDSA(
            const ptr<BlockProposal> &_proposal, const string &_hashStr, const string &_signature);

    void verifyECDSA(
            BLAKE3Hash &_hash, const string &_sig, const string &_publicKey);

    void verifySessionSigAndKey(BLAKE3Hash &_hash, const string &_sig,
                                const string &_publicKey, const string &pkSig, block_id _blockID, pair<node_id, node_id> _nodeId,
                                uint64_t _timeStamp);


    void verifyBlsSigShare(ptr<BLSSigShare> _sigShare, BLAKE3Hash &_hash);



    ptr<ThresholdSigShareSet> createSigShareSet(block_id _blockId);

    ptr<ThresholdSigShareSet> createDAProofSigShareSet(block_id _blockId, uint64_t _timestamp);

    ptr<ThresholdSigShare> createSigShare(const string &_sigShare, schain_id _schainID,
                                          block_id _blockID, schain_index _signerIndex, bool _forceMockup);

    ptr<ThresholdSigShare> createDAProofSigShare(const string &_sigShare, schain_id _schainID,
                                                 block_id _blockID, schain_index _signerIndex, uint64_t _timestamp, bool _forceMockup);

    void signProposal(BlockProposal *_proposal);



    tuple<ptr<ThresholdSigShare>, string, string, string> signDAProof(
            const ptr<BlockProposal> &_p);

    ptr<ThresholdSigShare> signBinaryConsensusSigShare(
            BLAKE3Hash &_hash, block_id _blockId, uint64_t _round);

    ptr<ThresholdSigShare> signBlockSigShare(BLAKE3Hash &_hash, block_id _blockId);

    tuple<string, string, string> signNetworkMsg(NetworkMessage &_msg);

    void verifyNetworkMsg(NetworkMessage &_msg);

    static ptr<void> decodeSGXPublicKey(const string &_keyHex);

    static pair<string, string> generateSGXECDSAKey(const ptr<StubClient> &_c);

    static string getSGXEcdsaPublicKey(const string &_keyName, const ptr<StubClient> &_c);

    static void generateSSLClientCertAndKey(string &_fullPathToDir);

    static void setSGXKeyAndCert(string &_keyFullPath, string &_certFullPath, uint64_t _sgxPort);


    string sgxSignECDSA(BLAKE3Hash &_hash, string &_keyName);

    tuple<string, string, string> signSessionECDSA(
            BLAKE3Hash &_hash, block_id _blockID);



    pair<ptr<BLSPublicKey>, ptr<BLSPublicKey >> getSgxBlsPublicKey(uint64_t _timestamp = 0);

    string getSgxBlsKeyName();

    static const string &getSgxUrl();

    static void setSgxUrl(const string &sgxUrl);

    static BLAKE3Hash calculatePublicKeyHash(string publicKey, block_id _blockID);



    void exitZMQClient();

    static void addECDSASignStats(uint64_t _time);

    static void addBLSSignStats(uint64_t _time);

    static uint64_t getEcdsaStats() {
        return ecdsaSignTotal / LEVELDB_STATS_HISTORY;
    }

    static uint64_t getBLSStats() {
        return blsSignTotal / LEVELDB_STATS_HISTORY;
    }

    static uint64_t getECDSAs() {
        return ecdsaCounter;
    }

    static uint64_t getBLSs() {
        return blsCounter;
    }

    static uint64_t getECDSATotals() {
        return ecdsaCounter;
    }

    static uint64_t getBLSTotals() {
        return blsCounter;
    }

    uint64_t getZMQSocketCount();

    bool isSGXServerDown();

    string signOracleResult(string _text);

    static string hashForOracle(char* _data, size_t _size);

    void checkZMQStatusIfUnknownECDSA(const string &_keyName);

    void checkZMQStatusIfUnknownBLS();

    string getECDSAPublicKeyForNodeId(const node_id &_nodeId, uint64_t _timeStamp);

    string getECDSAHistoricPublicKeyForNodeId(uint64_t _nodeId, uint64_t _timeStamp);

    pair<node_id, node_id> getHistoricNodeIDByIndex(uint64_t schain_id, uint64_t _timeStamp);
};

#define RETRY_BEGIN \
    CryptoManager::setRetryHappened(false); \
    while ( true ) {                      \
        try {
#define RETRY_END                                                                                  \
    ;                                                                                              \
    if ( CryptoManager::isRetryHappened() ) {                                                      \
        LOG( info, "Successfully reconnected to SGX server:"  );                                   \
        CryptoManager::setRetryHappened(false);                                                    \
    }                                                                                              \
    break;                                                                                         \
    }                                                                                              \
    catch ( const std::exception& e ) {                                                            \
        if ( e.what() && ( string( e.what() ).find( "Could not connect" ) != string::npos ||       \
                             string( e.what() ).find( "libcurl error: 56" ) != string::npos ||     \
                             string( e.what() ).find( "libcurl error: 35" ) != string::npos ||     \
                             string( e.what() ).find( "libcurl error: 52" ) != string::npos ||     \
                             string( e.what() ).find( "timed out" ) != string::npos ) ) {          \
            if ( string( e.what() ).find( "libcurl error: 52" ) != string::npos ) {                \
                LOG( err,                                                                          \
                    "Got libcurl error 52. You may be trying to connect with http to https "       \
                    "server" );                                                                    \
            };                                                                                     \
            if (!CryptoManager::isRetryHappened())                                                                                       \
                LOG( err, "Could not connect to sgx server: "                                      \
                          ", retrying each five seconds ... \n"  );                                \
            CryptoManager::setRetryHappened(true);                                                 \
            sleep( 5 );                                                                            \
        } else {                                                                                   \
            LOG( err, "Could not connect to sgx server: "  );                                      \
            throw;                                                                                 \
        }                                                                                          \
    }                                                                                              \
    catch ( ... ) {                                                                                \
        LOG( err, "FATAL Unknown error while connecting to sgx server:"  );                        \
        throw;                                                                                     \
    }                                                                                              \
    }

#endif  // SKALED_CRYPTOMANAGER_H
