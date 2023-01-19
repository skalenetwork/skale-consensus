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


#include <boost/asio.hpp>

#include "openssl/bio.h"


#include <cryptopp/eccrypto.h>
#include <cryptopp/hex.h>
#include <cryptopp/oids.h>

#include <cryptopp/keccak.h>
#include <cryptopp/sha.h>
#include <cryptopp/sha3.h>


#pragma GCC diagnostic push

#include "JsonStubClient.h"

#pragma GCC diagnostic pop

#include <jsonrpccpp/client/connectors/httpclient.h>

#include <sys/types.h>

#include "sgxwallet/stubclient.h"

#include "Log.h"
#include "SkaleCommon.h"

#include <gmp.h>
#include <network/ClientSocket.h>

#include "BLAKE3Hash.h"
#include "ConsensusBLSSigShare.h"
#include "ConsensusBLSSignature.h"
#include "ConsensusEdDSASigShareSet.h"
#include "ConsensusEdDSASignature.h"
#include "ConsensusSigShareSet.h"
#include "MockupSigShare.h"
#include "MockupSigShareSet.h"
#include "MockupSignature.h"

#include "chains/Schain.h"
#include "messages/NetworkMessage.h"

#include "ConsensusEdDSASigShare.h"
#include "bls/BLSPrivateKeyShare.h"
#include "bls/BLSPublicKeyShare.h"
#include "datastructures/CommittedBlock.h"
#include "monitoring/LivelinessMonitor.h"
#include "node/Node.h"
#include "node/NodeInfo.h"

#include "exceptions/InvalidSignatureException.h"
#include "json/JSONFactory.h"

#include "network/Utils.h"

#include "utils/Time.h"


#include "OpenSSLECDSAKey.h"
#include "OpenSSLEdDSAKey.h"


#include "CryptoManager.h"

void CryptoManager::initSGXClient() {
    if (isSGXEnabled) {
        if (isHTTPSEnabled) {
            if (isSSLCertEnabled) {
                LOG(info, string("Setting sgxSSLKeyFileFullPath to ") + sgxSSLKeyFileFullPath);
                LOG(
                        info, string("Setting sgxCertKeyFileFullPath to ") + sgxSSLCertFileFullPath);
                setSGXKeyAndCert(sgxSSLKeyFileFullPath, sgxSSLCertFileFullPath, sgxPort);
            } else {
                LOG(info, string("Setting sgxSSLKeyCertFileFullPath  is not set."
                                 "Assuming SGX server does not require client certs"));
            }
        }


        zmqClient = make_shared<SgxZmqClient>(sChain, sgxDomainName, 1031,
                                              this->isSSLCertEnabled, sgxSSLCertFileFullPath, sgxSSLKeyFileFullPath);
    }
}

string blsKeyToString(ptr<BLSPublicKey> _pk) {
    CHECK_ARGUMENT(_pk)
    auto vectorCoordinates = _pk->toString();
    CHECK_STATE(vectorCoordinates);

    string str;
    for (const auto &coord: *vectorCoordinates) {
        str.append(coord);
        str.append(":");
    }
    return str;
}

pair<ptr<BLSPublicKey>, ptr<BLSPublicKey> > CryptoManager::getSgxBlsPublicKey(
        uint64_t _timestamp) {
    LOG(debug, string("Looking for BLS public key for timestamp ") +
               std::to_string(_timestamp) +
               string(" to verify a block came through catchup"));
    if (_timestamp == uint64_t(-1) || previousBlsPublicKeys->size() < 2) {
        CHECK_STATE(sgxBLSPublicKey)
        LOG(debug, string("Got current BLS public key ") + blsKeyToString(sgxBLSPublicKey));
        return {sgxBLSPublicKey, nullptr};
    } else {
        // second key is used when the sig corresponds
        // to the last block before node rotation!
        // in this case we use the key for the group before

        // could not return iterator to end()
        // because finish ts for the current group equals uint64_t(-1)
        auto it = previousBlsPublicKeys->upper_bound(_timestamp);

        if (it == previousBlsPublicKeys->begin()) {
            LOG(debug, string("Got first BLS public key ") + blsKeyToString((*it).second));
            // if begin() then no previous groups for this key
            return {(*it).second, nullptr};
        }

        LOG(debug, string("Got two BLS public keys ") + blsKeyToString((*it).second) + " " +
                   blsKeyToString((*std::prev(it)).second));
        return {(*it).second, (*(--it)).second};
    }
}

string CryptoManager::getSgxBlsKeyName() {
    CHECK_STATE(!sgxBlsKeyName.empty());
    return sgxBlsKeyName;
}

CryptoManager::CryptoManager(uint64_t _totalSigners, uint64_t _requiredSigners, bool _isSGXEnabled,
                             string _sgxURL, string _sgxSslKeyFileFullPath, string _sgxSslCertFileFullPath,
                             string _sgxEcdsaKeyName, ptr<vector<string> > _sgxEcdsaPublicKeys)
        : sessionKeys(SESSION_KEY_CACHE_SIZE), sessionPublicKeys(SESSION_PUBLIC_KEY_CACHE_SIZE) {
    CHECK_ARGUMENT(_totalSigners >= _requiredSigners);


    historicECDSAPublicKeys = make_shared<map<uint64_t, string> >();
    historicNodeGroups = make_shared<map<uint64_t, vector<uint64_t> > >();

    totalSigners = _totalSigners;
    requiredSigners = _requiredSigners;

    isSGXEnabled = _isSGXEnabled;

    if (_isSGXEnabled) {
        CHECK_ARGUMENT(_sgxURL != "");
        CHECK_ARGUMENT(_sgxEcdsaKeyName != "");
        CHECK_ARGUMENT(_sgxEcdsaPublicKeys);

        sgxURL = _sgxURL;
        sgxSSLKeyFileFullPath = _sgxSslKeyFileFullPath;
        sgxSSLCertFileFullPath = _sgxSslCertFileFullPath;
        sgxECDSAKeyName = _sgxEcdsaKeyName;
        sgxECDSAPublicKeys = _sgxEcdsaPublicKeys;
    }

    initSGXClient();
}


pair<string, uint64_t> CryptoManager::parseSGXDomainAndPort(const string &_url) {
    CHECK_ARGUMENT(_url != "");
    size_t found = _url.find_first_of(":");

    if (found == string::npos) {
        BOOST_THROW_EXCEPTION(
                InvalidStateException("SGX URL does not include port " + _url, __CLASS_NAME__));
    }


    string end = _url.substr(found + 3);

    size_t found1 = end.find_first_of(":");

    if (found1 == string::npos) {
        found1 = end.size();
    }


    string domain = end.substr(0, found1);

    string port = end.substr(found1 + 1, end.size() - found1);


    uint64_t result;

    try {
        result = stoi(port);
    } catch (...) {
        throw_with_nested(
                InvalidStateException("Could not find port in URL " + _url, __CLASS_NAME__));
    }
    return {domain, result};
}


CryptoManager::CryptoManager(Schain &_sChain)
        : sessionKeys(SESSION_KEY_CACHE_SIZE),
          sessionPublicKeys(SESSION_PUBLIC_KEY_CACHE_SIZE),
          sChain(&_sChain) {
    totalSigners = getSchain()->getTotalSigners();
    requiredSigners = getSchain()->getRequiredSigners();

    historicECDSAPublicKeys = make_shared<map<uint64_t, string> >();


    CHECK_ARGUMENT(totalSigners >= requiredSigners);

    isSGXEnabled = _sChain.getNode()->isSgxEnabled();
    LOG(info, "SGX Enabled:" + to_string(isSGXEnabled));
    isSyncNode = _sChain.getNode()->isSyncOnlyNode();
    LOG(info, "Is Sync Node:" + to_string(isSyncNode));
    // we verify real signatures if sgx is enabled on a core node or if a sync node has
    // bls public key
    verifyRealSignatures = _sChain.getNode()->verifyRealSignatures();
    LOG(info, "Verify real signatures:" + to_string(verifyRealSignatures));


    // if we are going to verify ECDSA and BLS sigs we need to set up all coresponding objects
    if (verifyRealSignatures) {

        auto node = _sChain.getNode();
        sgxECDSAPublicKeys = node->getEcdsaPublicKeys();
        sgxBLSPublicKey = node->getBlsPublicKey();
        previousBlsPublicKeys = node->getPreviousBLSPublicKeys();
        historicECDSAPublicKeys = node->getHistoricECDSAPublicKeys();
        historicNodeGroups = node->getHistoricNodeGroups();

        CHECK_STATE(sgxECDSAPublicKeys);
        CHECK_STATE(sgxBLSPublicKey);

        // populate ecdsa key shares map
        for (uint64_t i = 0; i < (uint64_t) getSchain()->getNodeCount(); i++) {
            auto nodeId = getSchain()->getNode()->getNodeInfoByIndex(i + 1)->getNodeID();
            ecdsaPublicKeyMap[(uint64_t) nodeId] = sgxECDSAPublicKeys->at(i);
        }

    }



    // if we are on a core node and SGX is enabled we need to set up all objects
    // required to interact with the SGX server
    if (isSGXEnabled) {
        auto node = _sChain.getNode();
        sgxURL = node->getSgxUrl();
        if (sgxURL.back() == '/') {
            sgxURL = sgxURL.substr(0, sgxURL.size() - 1);
        }


        tie(sgxDomainName, sgxPort) = parseSGXDomainAndPort(sgxURL);


        sgxSSLCertFileFullPath = node->getSgxSslCertFileFullPath();
        sgxSSLKeyFileFullPath = node->getSgxSslKeyFileFullPath();
        sgxECDSAKeyName = node->getEcdsaKeyName();
        sgxBlsKeyName = node->getBlsKeyName();
        sgxBLSPublicKeyShares = node->getBlsPublicKeys();
        tie(sgxDomainName, sgxPort) = parseSGXDomainAndPort(sgxURL);

        CHECK_STATE(sgxURL != "");
        CHECK_STATE(sgxECDSAKeyName != "");
        CHECK_STATE(sgxBlsKeyName != "");
        CHECK_STATE(sgxBLSPublicKeyShares);

        CHECK_STATE(JSONFactory::splitString(sgxBlsKeyName)->size() == 7);
        CHECK_STATE(JSONFactory::splitString(sgxECDSAKeyName)->size() == 2);

        isHTTPSEnabled = sgxURL.find("https:/") != string::npos;

        isSSLCertEnabled =
                (!sgxSSLKeyFileFullPath.empty()) && (!sgxSSLCertFileFullPath.empty());

        initSGXClient();


        // populate bls key shares map
        for (uint64_t i = 0; i < (uint64_t) getSchain()->getNodeCount(); i++) {
            blsPublicKeySharesMapByIndex[i + 1] = sgxBLSPublicKeyShares->at(i);
        }


        // verify that the SGX ecdsa public key of the current node on SGX server equal to the key in config
        auto ecdsaPublicKeyOnServer = getSGXEcdsaPublicKey(sgxECDSAKeyName, getSgxClient());
        auto ecdsaPublicKeyInConfig = ecdsaPublicKeyMap.at((uint64_t) getSchain()->getNode()->getNodeID());
        if (ecdsaPublicKeyOnServer != ecdsaPublicKeyInConfig) {
            BOOST_THROW_EXCEPTION(InvalidStateException(
                                          "Misconfiguration. \n Configured ECDSA public key for this node \n" +
                                          ecdsaPublicKeyInConfig +
                                          " \n is not equal to the public key for \n " + sgxECDSAKeyName +
                                          "\n  on the SGX server: \n" + ecdsaPublicKeyOnServer,
                                                  __CLASS_NAME__ ));
        };

    }


    auto cfg = getSchain()->getNode()->getCfg();

    if (cfg.find("simulateBLSSigFailBlock") != cfg.end()) {
        simulateBLSSigFailBlock = cfg.at("simulateBLSSigFailBlock").get<uint64_t>();
    }


}

void CryptoManager::setSGXKeyAndCert(
        string &_keyFullPath, string &_certFullPath, uint64_t _sgxPort) {
    jsonrpc::HttpClient::setKeyFileFullPath(_keyFullPath);
    jsonrpc::HttpClient::setCertFileFullPath(_certFullPath);
    jsonrpc::HttpClient::setSslClientPort(_sgxPort);
}

Schain *CryptoManager::getSchain() const {
    return sChain;
}


MPZNumber::MPZNumber() {
    mpz_init(this->number);
}

MPZNumber::~MPZNumber() {
    mpz_clear(this->number);
}

using namespace std;
unsigned long long int random_value = 0;  // Declare value to store data into
size_t size = sizeof(random_value);     // Declare size of data


ifstream CryptoManager::urandom("/dev/urandom", ios::in | ios::binary);  // Open stream

std::tuple<ptr<OpenSSLEdDSAKey>, string> CryptoManager::localGenerateFastKey() {
    auto key = OpenSSLEdDSAKey::generateKey();
    auto pKey = key->serializePubKey();
    return {key, pKey};
}


tuple<string, string, string> CryptoManager::signSessionECDSA(
        BLAKE3Hash &_hash, block_id _blockID) {
    ptr<OpenSSLEdDSAKey> privateKey = nullptr;
    string publicKey = "";
    string pkSig = "";


    {
        LOCK(sessionKeysLock);


        if (auto result = sessionKeys.getIfExists((uint64_t) _blockID); result.has_value()) {
            tie(privateKey, publicKey, pkSig) =
                    any_cast<tuple<ptr<OpenSSLEdDSAKey>, string, string> >(result);
            CHECK_STATE(privateKey);
            CHECK_STATE(publicKey != "");
            CHECK_STATE(pkSig != "");

        } else {
            tie(privateKey, publicKey) = localGenerateFastKey();

            BLAKE3Hash pKeyHash = calculatePublicKeyHash(publicKey, _blockID);

            CHECK_STATE(sgxECDSAKeyName != "");
            pkSig = sgxSignECDSA(pKeyHash, sgxECDSAKeyName);
            CHECK_STATE(pkSig != "");

            sessionKeys.put((uint64_t) _blockID, {privateKey, publicKey, pkSig});
        }
    }

    auto ret = privateKey->sign((const char *) _hash.data());

    return {ret, publicKey, pkSig};
}

BLAKE3Hash CryptoManager::calculatePublicKeyHash(const string publicKey, block_id _blockID) {
    auto bytesToHash = make_shared<vector<uint8_t> >();

    auto bId = (uint64_t) _blockID;
    auto bidP = (uint8_t *) &bId;

    for (uint64_t i = 0; i < sizeof(uint64_t); i++) {
        bytesToHash->push_back(bidP[i]);
    }

    for (uint64_t i = 0; i < publicKey.size(); i++) {
        bytesToHash->push_back(publicKey.at(i));
    }

    return BLAKE3Hash::calculateHash(bytesToHash);
}


string CryptoManager::sgxSignECDSA(BLAKE3Hash &_hash, string &_keyName) {
    string ret;


    checkZMQStatusIfUnknownECDSA(_keyName);


    // temporary solution to support old servers
    if (zmqClient->getZMQStatus() == SgxZmqClient::TRUE) {
        ret = zmqClient->ecdsaSignMessageHash(16, _keyName, _hash.toHex(), false);
    } else {
        Json::Value result;
        RETRY_BEGIN
                getSchain()->getNode()->exitCheck();
                result = getSgxClient()->ecdsaSignMessageHash(16, _keyName, _hash.toHex());RETRY_END
        JSONFactory::checkSGXStatus(result);

        string r = JSONFactory::getString(result, "signature_r");
        string v = JSONFactory::getString(result, "signature_v");
        string s = JSONFactory::getString(result, "signature_s");
        ret = v + ":" + r.substr(2) + ":" + s.substr(2);
    }


    return ret;
}


void CryptoManager::verifyECDSA(BLAKE3Hash &_hash, const string &_sig, const string &_publicKey) {
    auto key = OpenSSLECDSAKey::importSGXPubKey(_publicKey);

    try {
        key->verifySGXSig(_sig, (const char *) _hash.data());
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}

string CryptoManager::sign(BLAKE3Hash &_hash) {
    string result;

    uint64_t time = 0;
    ecdsaCounter.fetch_add(1);
    auto measureTime = (ecdsaCounter % 100 == 0);
    if (measureTime)
        time = Time::getCurrentTimeMs();

    if (isSGXEnabled) {
        CHECK_STATE(sgxECDSAKeyName != "")
        result = sgxSignECDSA(_hash, sgxECDSAKeyName);
    } else {
        result = _hash.toHex();
    }

    if (measureTime)
        addECDSASignStats(Time::getCurrentTimeMs() - time);

    return result;
}

string CryptoManager::signOracleResult(string _text) {
    string result;

    auto hashStr = hashForOracle(_text.data(), _text.size());

    if (isSGXEnabled) {
        CHECK_STATE(sgxECDSAKeyName != "")
        result = zmqClient->ecdsaSignMessageHash(16, sgxECDSAKeyName, hashStr, true);
    } else {
        result = hashStr;
    }

    return result;
}


string CryptoManager::hashForOracle(char *_data, size_t _size) {

    CHECK_ARGUMENT(_data);
    try {
        CryptoPP::Keccak_256 hash;
        string digest;


        hash.Update((const CryptoPP::byte *) _data, _size);
        digest.resize(hash.DigestSize());
        hash.Final((CryptoPP::byte *) &digest[0]);

        return Utils::carray2Hex((const uint8_t *) digest.data(), HASH_LEN);
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}

tuple<string, string, string> CryptoManager::signSession(BLAKE3Hash &_hash, block_id _blockId) {
    if (isSGXEnabled) {
        string signature = "";
        string pubKey = "";
        string pkSig = "";

        tie(signature, pubKey, pkSig) = signSessionECDSA(_hash, _blockId);
        CHECK_STATE(signature != "");
        CHECK_STATE(pubKey != "");
        CHECK_STATE(pkSig != "");
        return {signature, pubKey, pkSig};
    } else {
        return {_hash.toHex(), "", ""};
    }
}


void CryptoManager::verifySessionEdDSASig(
        BLAKE3Hash &_hash, const string &_sig, const string &_publicKey) {
    try {
        CHECK_ARGUMENT(_sig != "")


        if (isSGXEnabled) {
            auto pkey = OpenSSLEdDSAKey::importPubKey(_publicKey);
            try {
                pkey->verifySig(_sig, (const char *) _hash.data());
            } catch (...) {
                throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__
                                                                      + string(" Could not verify EdDSA sig")));
            }
        } else if (!getSchain()->getNode()->isSyncOnlyNode()) {
            // mockup - used for testing
            if (_sig.find(":") != string::npos) {
                LOG(critical,
                    "Misconfiguration: this node is in mockup signature mode,"
                    "but other node sent a real signature ");
                exit(-1);
            }
            CHECK_STATE2(_sig == _hash.toHex(), "Mockup signature verification failed");
        }

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


void CryptoManager::verifyECDSASig(
        BLAKE3Hash &_hash, const string &_sig, node_id _nodeId, uint64_t _timeStamp) {
    CHECK_ARGUMENT(!_sig.empty())

    try {
        if (isSGXEnabled) {
            string pubKey;

            pubKey = getECDSAPublicKeyForNodeId(_nodeId, _timeStamp);

            CHECK_STATE2(
                    !pubKey.empty(), "Sig verification failed: could not find ECDSA key for nodeId");

            try {
                verifyECDSA(_hash, _sig, pubKey);
            } catch (...) {
                throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
            }


        } else if (!getSchain()->getNode()->isSyncOnlyNode()) {
            // mockup - used for testing
            if (_sig.find(":") != string::npos) {
                LOG(critical,
                    "Misconfiguration: this node is in mockup signature mode,"
                    "but other node sent a real signature ");
                exit(-1);
            }
            CHECK_STATE2(_sig == _hash.toHex(), "Mockup signature verification failed");
        }

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}

// get ECDSA public key for nodeID
string CryptoManager::getECDSAPublicKeyForNodeId(const node_id &_nodeId, uint64_t _timeStamp) {
    string result;

    // check if the node is member of the current node set

    {
        LOCK(ecdsaPublicKeyMapLock)

        if (ecdsaPublicKeyMap.count((uint64_t) _nodeId) > 0) {
            // nodeId found in the current set of nodes
            result = ecdsaPublicKeyMap.at((uint64_t) _nodeId);
            return result;
        }
    }

    // could not find nodeId in the current 16 node chain
    // this means the node was probably part of the chain before one of rotations

    // get key from rotation history
    // return empty string if key is not found

    return getECDSAHistoricPublicKeyForNodeId((uint64_t) _nodeId, _timeStamp);
}


// get ECDSA public key for nodeID and time stamp. Time stamp (uint64_t)-1 is current time.
// If not found, return empty string;
string CryptoManager::getECDSAHistoricPublicKeyForNodeId(uint64_t _nodeId, uint64_t _timeStamp) {
    LOCK(historicEcdsaPublicKeyMapLock);

    vector<uint64_t> nodeIdsInGroup;
    if (_timeStamp == uint64_t(-1)) {
        if (historicNodeGroups->count(_timeStamp) > 0) {
            nodeIdsInGroup = historicNodeGroups->at(_timeStamp);
        } else {
            LOG(err, "Could not find nodeIds for ECDSA");
            return "";
        }
    } else {
        nodeIdsInGroup = (*historicNodeGroups->upper_bound(_timeStamp)).second;
    }

    if (find(nodeIdsInGroup.begin(), nodeIdsInGroup.end(), _nodeId) == nodeIdsInGroup.end()) {
        LOG(err, "Could not find node in the ECDSA group for this timeStamp");
        return "";
    }

    if (historicECDSAPublicKeys->count(_nodeId) > 0) {
        return historicECDSAPublicKeys->at(_nodeId);
    } else {
        LOG(err, "Could not find nodeId in historic ECDSA public keys");
        return "";
    }
}


tuple<ptr<ThresholdSigShare>, string, string, string> CryptoManager::signDAProof(
        const ptr<BlockProposal> &_p) {
    CHECK_ARGUMENT(_p);


    auto h = _p->getHash();

    auto sigShare = signDAProofSigShare(h, _p->getBlockID(), false);
    CHECK_STATE(sigShare);

    auto combinedHash = BLAKE3Hash::merkleTreeMerge(_p->getHash(), sigShare->computeHash());
    auto [ecdsaSig, pubKey, pubKeySig] = signSession(combinedHash, _p->getBlockID());
    return {sigShare, ecdsaSig, pubKey, pubKeySig};
}


ptr<ThresholdSigShare> CryptoManager::signBinaryConsensusSigShare(
        BLAKE3Hash &_hash, block_id _blockId, uint64_t _round) {
    auto result = signSigShare(_hash, _blockId, ((uint64_t) _round) < COMMON_COIN_ROUND);
    CHECK_STATE(result);
    return result;
}

ptr<ThresholdSigShare> CryptoManager::signBlockSigShare(BLAKE3Hash &_hash, block_id _blockId) {
    auto result = signSigShare(_hash, _blockId, false);
    CHECK_STATE(result);
    return result;
}

void CryptoManager::verifyBlockSig(
        string &_sigStr, block_id _blockId, BLAKE3Hash &_hash, const TimeStamp &_ts) {
    try {
        if (simulateBLSSigFailBlock > 0) {
            if (simulateBLSSigFailBlock == (uint64_t) _blockId) {
                throw InvalidSignatureException("Simulated sig fail", __CLASS_NAME__);
            }
        }

        if (verifyRealSignatures) {
            auto _signature = make_shared<ConsensusBLSSignature>(
                    _sigStr, _blockId, totalSigners, requiredSigners);

            verifyThresholdSig(_signature, _hash, _ts);
        }

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


ptr<ThresholdSigShare> CryptoManager::signDAProofSigShare(
        BLAKE3Hash &_hash, block_id _blockId, bool _forceMockup) {
    MONITOR(__CLASS_NAME__, __FUNCTION__)

    if (getSchain()->getNode()->isSgxEnabled() && !_forceMockup) {
        auto &&[sig, publicKey, pkSig] = this->signSessionECDSA(_hash, _blockId);

        auto share = to_string((uint64_t) getSchain()->getSchainIndex()) + ";" + sig + ";" +
                     publicKey + ";" + pkSig;

        return make_shared<ConsensusEdDSASigShare>(share, sChain->getSchainID(), _blockId,
                                                   totalSigners);

    } else {
        auto sigShare = _hash.toHex();
        return make_shared<MockupSigShare>(sigShare, sChain->getSchainID(), _blockId,
                                           sChain->getSchainIndex(), sChain->getTotalSigners(),
                                           sChain->getRequiredSigners());
    }
}

void CryptoManager::verifyDAProofSigShare(ptr<ThresholdSigShare> _sigShare,
                                          schain_index _schainIndex, BLAKE3Hash &_hash, node_id _nodeId,
                                          bool _forceMockup) {
    MONITOR(__CLASS_NAME__, __FUNCTION__)

    try {
        if (getSchain()->getNode()->isSgxEnabled() && !_forceMockup) {
            auto sShare = dynamic_pointer_cast<ConsensusEdDSASigShare>(_sigShare);

            CHECK_STATE(sShare);

            CHECK_STATE2(sShare->getSignerIndex() == _schainIndex,
                         "Incorrect schain index in sigShare:" + to_string(sShare->getSignerIndex()))

            sShare->verify(*this, _hash, _nodeId);


            return;

        } else {
            return;
        }

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


ptr<ThresholdSigShare> CryptoManager::signSigShare(
        BLAKE3Hash &_hash, block_id _blockId, bool _forceMockup) {
    MONITOR(__CLASS_NAME__, __FUNCTION__)

    ptr<ThresholdSigShare> result = nullptr;

    uint64_t time = 0;
    blsCounter.fetch_add(1);

    auto measureTime = (blsCounter % 100 == 0);
    if (measureTime)
        time = Time::getCurrentTimeMs();

    if (getSchain()->getNode()->isSgxEnabled() && !_forceMockup) {
        Json::Value jsonShare;
        string ret;


        checkZMQStatusIfUnknownBLS();

        if (zmqClient->getZMQStatus() == SgxZmqClient::TRUE) {
            ret = zmqClient->blsSignMessageHash(
                    getSgxBlsKeyName(), _hash.toHex(), requiredSigners, totalSigners, false);
        } else {
            RETRY_BEGIN
                    getSchain()->getNode()->exitCheck();
                    jsonShare = getSgxClient()->blsSignMessageHash(
                            getSgxBlsKeyName(), _hash.toHex(), requiredSigners, totalSigners);RETRY_END

            JSONFactory::checkSGXStatus(jsonShare);
            ret = JSONFactory::getString(jsonShare, "signatureShare");
        }

        auto sigShare = make_shared<string>(ret);

        auto sig = make_shared<BLSSigShare>(
                sigShare, (uint64_t) getSchain()->getSchainIndex(), requiredSigners, totalSigners);
        result = make_shared<ConsensusBLSSigShare>(sig, sChain->getSchainID(), _blockId);

    } else {
        auto sigShare = _hash.toHex();
        result = make_shared<MockupSigShare>(sigShare, sChain->getSchainID(), _blockId,
                                             sChain->getSchainIndex(), sChain->getTotalSigners(),
                                             sChain->getRequiredSigners());
    }

    if (measureTime)
        addBLSSignStats(Time::getCurrentTimeMs() - time);

    return result;
}

void CryptoManager::verifyThresholdSig(ptr<ThresholdSignature> _signature, BLAKE3Hash &_hash, const TimeStamp &_ts) {
    try {
        CHECK_STATE(_signature);


        MONITOR(__CLASS_NAME__, __FUNCTION__)

        if (verifyRealSignatures) {
            auto blsSig = dynamic_pointer_cast<ConsensusBLSSignature>(_signature);

            CHECK_STATE(blsSig);

            auto blsKeys = getSgxBlsPublicKey(_ts.getS());

            auto libBlsSig = blsSig->getBlsSig();


            if (!blsKeys.first->VerifySig(
                    make_shared<array<uint8_t, HASH_LEN> >(_hash.getHash()), libBlsSig)) {
                LOG(err, "Could not BLS verify signature:" + _signature->toString() +
                         string(":KEY:") + blsKeys.first->toString()->at(0) +
                         ":HASH:" + _hash.toHex());

                // second key is used when the sig corresponds
                // to the last block before node rotation!
                // in this case we use the key for the group before


                CHECK_STATE2(blsKeys.second, "BLS signature verification failed");
                CHECK_STATE2(
                        blsKeys.second->VerifySig(
                                make_shared<array<uint8_t, HASH_LEN> >(_hash.getHash()), libBlsSig),
                        "BLS sig verification failed using both current and previous key");
            }

        } else {
            // mockups sigs are not verified
        }

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


// Verify threshold sig share using the current set of BLS keys.
// Since threshold sig shares are glued for the current block
// historic keys are not needed in this case.
// throw an exception if the share does not verify

void CryptoManager::verifyThresholdSigShare(
        ptr<ThresholdSigShare> _sigShare, BLAKE3Hash &_hash) {
    CHECK_STATE(_sigShare);
    // sync nodes do not do sig gluing
    CHECK_STATE(!getSchain()->getNode()->isSyncOnlyNode())

    MONITOR(__CLASS_NAME__, __FUNCTION__)

    try {
        if ((getSchain()->getNode()->isSgxEnabled())) {
            auto consensusBlsSigShare = dynamic_pointer_cast<ConsensusBLSSigShare>(_sigShare);

            CHECK_STATE(consensusBlsSigShare);

            ptr<BLSSigShare> blsSigShare = consensusBlsSigShare->getBlsSigShare();

            verifyBlsSigShare(blsSigShare, _hash);

        } else {
            // mockup sigshares are not verified
        }
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


// Verify BLS sig share using the current set of BLS keys.
// Since threshold sig shares are glued for the current block
// historic keys are not needed in this case.
// throw an exception if the share does not verify
void CryptoManager::verifyBlsSigShare(ptr<BLSSigShare> _sigShare, BLAKE3Hash &_hash) {
    CHECK_STATE(_sigShare);

    try {
        CHECK_STATE(blsPublicKeySharesMapByIndex.size() == getSchain()->getNodeCount());
        CHECK_STATE(blsPublicKeySharesMapByIndex.count(_sigShare->getSignerIndex() > 0));


        auto blsPublicKeyShare =
                BLSPublicKeyShare(blsPublicKeySharesMapByIndex.at(_sigShare->getSignerIndex()),
                                  requiredSigners, totalSigners);

        bool res = false;

        try {
            res = blsPublicKeyShare.VerifySig(
                    std::make_shared<std::array<uint8_t, 32> >(_hash.getHash()), _sigShare,
                    requiredSigners, totalSigners);
        } catch (...) {
            LOG(err, "Bls sig share did not verify NODE_ID:" +
                     to_string((uint64_t) _sigShare->getSignerIndex()));
            throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
        }

        CHECK_STATE(res);
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


ptr<ThresholdSigShareSet> CryptoManager::createSigShareSet(block_id _blockId) {
    if (getSchain()->getNode()->isSgxEnabled()) {
        return make_shared<ConsensusSigShareSet>(_blockId, totalSigners, requiredSigners);
    } else {
        return make_shared<MockupSigShareSet>(_blockId, totalSigners, requiredSigners);
    }
}

ptr<ThresholdSigShareSet> CryptoManager::createDAProofSigShareSet(block_id _blockId) {
    if (getSchain()->getNode()->isSgxEnabled()) {
        return make_shared<ConsensusEdDSASigShareSet>(
                getSchain()->getSchainID(), _blockId, totalSigners, requiredSigners);
    } else {
        return make_shared<MockupSigShareSet>(_blockId, totalSigners, requiredSigners);
    }
}


ptr<ThresholdSigShare> CryptoManager::createSigShare(const string &_sigShare,
                                                     schain_id _schainID, block_id _blockID, schain_index _signerIndex,
                                                     bool _forceMockup) {
    CHECK_ARGUMENT(_sigShare != "");
    CHECK_STATE(totalSigners >= requiredSigners);


    if (getSchain()->getNode()->isSgxEnabled() && !_forceMockup) {
        return make_shared<ConsensusBLSSigShare>(
                _sigShare, _schainID, _blockID, _signerIndex, totalSigners, requiredSigners);
    } else {
        return make_shared<MockupSigShare>(
                _sigShare, _schainID, _blockID, _signerIndex, totalSigners, requiredSigners);
    }
}


ptr<ThresholdSigShare> CryptoManager::createDAProofSigShare(const string &_sigShare,
                                                            schain_id _schainID, block_id _blockID,
                                                            schain_index _signerIndex, bool _forceMockup) {
    CHECK_ARGUMENT(!_sigShare.empty());

    if (getSchain()->getNode()->isSgxEnabled() && !_forceMockup) {
        auto result = make_shared<ConsensusEdDSASigShare>(
                _sigShare, _schainID, _blockID, totalSigners);

        return result;

    } else {
        return make_shared<MockupSigShare>(
                _sigShare, _schainID, _blockID, _signerIndex,
                totalSigners, requiredSigners);
    }
}


void CryptoManager::signProposal(BlockProposal *_proposal) {
    MONITOR(__CLASS_NAME__, __FUNCTION__)

    CHECK_ARGUMENT(_proposal);
    auto h = _proposal->getHash();
    auto signature = sign(h);
    CHECK_STATE(signature != "");
    _proposal->addSignature(signature);
}

tuple<string, string, string> CryptoManager::signNetworkMsg(NetworkMessage &_msg) {
    MONITOR(__CLASS_NAME__, __FUNCTION__);
    auto h = _msg.getHash();
    auto &&[signature, publicKey, pkSig] = signSession(h, _msg.getBlockId());
    CHECK_STATE(signature != "");
    return {signature, publicKey, pkSig};
}


void CryptoManager::verifyNetworkMsg(NetworkMessage &_msg) {
    auto sig = _msg.getECDSASig();
    auto hash = _msg.getHash();
    auto publicKey = _msg.getPublicKey();
    auto pkSig = _msg.getPkSig();
    auto blockId = _msg.getBlockID();
    auto nodeId = _msg.getSrcNodeID();
    try {
        verifySessionSigAndKey(hash, sig, publicKey, pkSig, blockId, nodeId,
                               getSchain()->getLastCommittedBlockTimeStamp().getLinuxTimeMs());
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


void CryptoManager::verifySessionSigAndKey(BLAKE3Hash &_hash, const string &_sig,
                                           const string &_publicKey, const string &pkSig, block_id _blockID,
                                           node_id _nodeId,
                                           uint64_t _timeStamp) {
    MONITOR(__CLASS_NAME__, __FUNCTION__);


    CHECK_STATE(!_sig.empty());


    {
        LOCK(publicSessionKeysLock)

        if (auto result = sessionPublicKeys.getIfExists(pkSig); result.has_value()) {
            auto publicKey2 = any_cast<string>(result);
            CHECK_STATE(publicKey2 == _publicKey)
        } else {
            if (isSGXEnabled) {
                auto pkeyHash = calculatePublicKeyHash(_publicKey, _blockID);
                try {
                    verifyECDSASig(pkeyHash, pkSig, _nodeId, _timeStamp);
                } catch (...) {
                    LOG(err, "PubKey ECDSA sig did not verify NODE_ID:" +
                             to_string((uint64_t) _nodeId));
                    throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
                }
                sessionPublicKeys.put(pkSig, _publicKey);
            }
        }
    }

    try {
        verifySessionEdDSASig(_hash, _sig, _publicKey);
    } catch (...) {
        LOG(err, "verifySessionSigAndKey ECDSA sig did not verify");
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}

void CryptoManager::verifyProposalECDSA(
        const ptr<BlockProposal> &_proposal, const string &_hashStr, const string &_signature) {
    CHECK_ARGUMENT(_proposal);
    CHECK_ARGUMENT(_hashStr != "")
    CHECK_ARGUMENT(_signature != "")

    // default proposal is not signed using ECDSA
    CHECK_STATE(_proposal->getProposerIndex() != 0);
    auto hash = _proposal->getHash();

    CHECK_STATE2(hash.toHex() == _hashStr, "Incorrect proposal hash");

    try {
        verifyECDSASig(
                hash, _signature, _proposal->getProposerNodeID(), _proposal->getTimeStampMs());
    } catch (...) {
        LOG(err, "verifyProposalECDSA:  ECDSA sig did not verify");
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


ptr<ThresholdSignature> CryptoManager::verifyDAProofThresholdSig(
    BLAKE3Hash &_hash, const string &_signature, block_id _blockId) {
    MONITOR(__CLASS_NAME__, __FUNCTION__)


    CHECK_ARGUMENT(!_signature.empty());

    try {
        if (verifyRealSignatures) {
            auto sig = make_shared<ConsensusEdDSASignature>(
                    _signature, getSchain()->getSchainID(), _blockId, totalSigners,
                    requiredSigners);

            sig->verify(*this, _hash);

            return sig;
        } else {

            auto sig = make_shared<MockupSignature>(
                    _signature, _blockId, totalSigners, requiredSigners);

            // if we have a syncnode and verifyRealSignatures is false
            // we do not verify anything
            // if we have a core node and verifyRealSignatures is false
            // its means that we are running a test chain with mockup signatures
            // so we verify mockup signatures
            if (!this->isSyncNode) {
                CHECK_STATE2( sig->toString() == _hash.toHex(),
                    "Mockup da signature did not verify:SYNC_NODE:" +
                        to_string( this->isSyncNode ) + ":VRS:" + to_string(verifyRealSignatures));
            }

            return sig;
        }

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


using namespace CryptoPP;

ptr<void> CryptoManager::decodeSGXPublicKey(const string &_keyHex) {
    CHECK_ARGUMENT(_keyHex != "");

    HexDecoder decoder;
    CHECK_STATE(decoder.Put((unsigned char *) _keyHex.data(), _keyHex.size()) == 0);
    decoder.MessageEnd();
    CryptoPP::ECP::Point q;
    size_t len = decoder.MaxRetrievable();
    q.identity = false;
    q.x.Decode(decoder, len / 2);
    q.y.Decode(decoder, len / 2);

    auto publicKey = make_shared<ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::PublicKey>();
    publicKey->Initialize(ASN1::secp256r1(), q);
    return publicKey;
}


string CryptoManager::getSGXEcdsaPublicKey(const string &_keyName, const ptr<StubClient> &_c) {
    CHECK_ARGUMENT(_keyName != "");
    CHECK_ARGUMENT(_c);

    LOG(info, "Getting ECDSA public key for " + _keyName.substr(0, 8) + "...");

    Json::Value result;

    RETRY_BEGIN
            result = _c->getPublicECDSAKey(_keyName);RETRY_END

    JSONFactory::checkSGXStatus(result);

    auto publicKey = JSONFactory::getString(result, "publicKey");

    LOG(info, "Got ECDSA public key: " + publicKey);

    return publicKey;
}

pair<string, string> CryptoManager::generateSGXECDSAKey(const ptr<StubClient> &_c) {
    CHECK_ARGUMENT(_c);

    Json::Value result;
    RETRY_BEGIN
            result = _c->generateECDSAKey();RETRY_END
    JSONFactory::checkSGXStatus(result);

    auto keyName = JSONFactory::getString(result, "keyName");
    auto publicKey = JSONFactory::getString(result, "publicKey");

    CHECK_STATE(keyName.size() > 10);
    CHECK_STATE(publicKey.size() > 10);
    CHECK_STATE(keyName.find("NEK") != string::npos);

    auto publicKey2 = getSGXEcdsaPublicKey(keyName, _c);

    CHECK_STATE(publicKey2 != "");

    return {keyName, publicKey};
}


void CryptoManager::generateSSLClientCertAndKey(string &_fullPathToDir) {
    const std::string VALID_CHARS =
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<int> distribution(0, VALID_CHARS.size() - 1);
    std::string random_string;
    std::generate_n(std::back_inserter(random_string), 10,
                    [&]() { return VALID_CHARS[distribution(generator)]; });
    system(
            ("/usr/bin/openssl req -new -sha256 -nodes -out " + _fullPathToDir +
             "/csr  -newkey rsa:2048 -keyout " + _fullPathToDir + "/key -subj /CN=" + random_string)
                    .data());
    string str, csr;
    ifstream file;
    file.open(_fullPathToDir + "/csr");
    while (getline(file, str)) {
        csr.append(str);
        csr.append("\n");
    }

    jsonrpc::HttpClient client("http://localhost:1027");
    StubClient c(client, jsonrpc::JSONRPC_CLIENT_V2);

    Json::Value result;

    RETRY_BEGIN
            result = c.SignCertificate(csr);RETRY_END

    JSONFactory::checkSGXStatus(result);
    string certHash = JSONFactory::getString(result, "hash");

    RETRY_BEGIN
            result = c.GetCertificate(certHash);RETRY_END

    JSONFactory::checkSGXStatus(result);

    string signedCert = JSONFactory::getString(result, "cert");
    ofstream outFile;
    outFile.open(_fullPathToDir + "/cert");
    outFile << signedCert;
}


ptr<StubClient> CryptoManager::getSgxClient() {
    auto tid = (uint64_t) pthread_self();

    LOCK(clientsLock);

    if (httpClients.count(tid) == 0) {
        CHECK_STATE(sgxClients.count(tid) == 0);

        auto httpClient = make_shared<jsonrpc::HttpClient>(sgxURL);

        httpClients.insert({tid, httpClient});
        sgxClients.insert(
                {tid, make_shared<StubClient>(*httpClient, jsonrpc::JSONRPC_CLIENT_V2)});
    }

    return sgxClients.at(tid);
}

bool CryptoManager::retryHappened = false;

string CryptoManager::sgxURL = "";

bool CryptoManager::isRetryHappened() {
    return retryHappened;
}

void CryptoManager::setRetryHappened(bool _retryHappened) {
    CryptoManager::retryHappened = _retryHappened;
}

const string &CryptoManager::getSgxUrl() {
    return sgxURL;
}

void CryptoManager::setSgxUrl(const string &_sgxUrl) {
    sgxURL = _sgxUrl;
}


void CryptoManager::exitZMQClient() {
    LOG(info, "consensus engine exiting: SGXZMQClient exiting");
    if (isSGXEnabled && zmqClient)
        zmqClient->exit();
    LOG(info, "consensus engine exiting: SGXZMQClient exited");
}

list<uint64_t> CryptoManager::ecdsaSignTimes;
recursive_mutex CryptoManager::ecdsaSignMutex;
atomic<uint64_t> CryptoManager::ecdsaSignTotal = 0;

list<uint64_t> CryptoManager::blsSignTimes;
recursive_mutex CryptoManager::blsSignMutex;
atomic<uint64_t> CryptoManager::blsSignTotal = 0;

atomic<uint64_t> CryptoManager::blsCounter = 0;
atomic<uint64_t> CryptoManager::ecdsaCounter = 0;

void CryptoManager::addECDSASignStats(uint64_t _time) {
    ecdsaSignTotal.fetch_add(_time);
    LOCK(ecdsaSignMutex);
    ecdsaSignTimes.push_back(_time);
    if (ecdsaSignTimes.size() > LEVELDB_STATS_HISTORY) {
        ecdsaSignTotal.fetch_sub(ecdsaSignTimes.front());
        ecdsaSignTimes.pop_front();
    }
}

void CryptoManager::addBLSSignStats(uint64_t _time) {
    blsSignTotal.fetch_add(_time);
    LOCK(blsSignMutex);
    blsSignTimes.push_back(_time);
    if (blsSignTimes.size() > LEVELDB_STATS_HISTORY) {
        blsSignTotal.fetch_sub(blsSignTimes.front());
        blsSignTimes.pop_front();
    }
}

uint64_t CryptoManager::getZMQSocketCount() {
    if (!zmqClient)
        return 0;
    else
        return 1;
}

bool CryptoManager::isSGXServerDown() {
    if (!isSGXEnabled)
        return false;
    CHECK_STATE(zmqClient);
    return (zmqClient->isServerDown());
}


void CryptoManager::checkZMQStatusIfUnknownECDSA(const string &_keyName) {
    if (zmqClient->getZMQStatus() == SgxZmqClient::UNKNOWN) {
        static string sampleHash =
                "01020304050607080910111213141516171819202122232425262728"
                "29303132";
        try {
            auto ret1 = zmqClient->ecdsaSignMessageHash(16, _keyName, sampleHash, true);
            zmqClient->setZmqStatus(SgxZmqClient::TRUE);
            LOG(info, "Successfully connected to SGX ZMQ API.");
        } catch (...) {
            zmqClient->setZmqStatus(SgxZmqClient::FALSE);
            LOG(warn, "Could not connect SGX ZMQ API. Will fallback to HTTP(S)");
        }
    }
}


void CryptoManager::checkZMQStatusIfUnknownBLS() {
    if (zmqClient->getZMQStatus() == SgxZmqClient::UNKNOWN) {
        static string sampleHash =
                "01020304050607080910111213141516171819202122232425262728"
                "29303132";
        try {
            auto ret1 = zmqClient->blsSignMessageHash(
                    getSgxBlsKeyName(), sampleHash, requiredSigners, totalSigners, true);
            zmqClient->setZmqStatus(SgxZmqClient::TRUE);
            LOG(info, "Successfully connected to SGX ZMQ API.");
        } catch (...) {
            zmqClient->setZmqStatus(SgxZmqClient::FALSE);
            LOG(warn, "Could not connect SGX ZMQ API. Will fallback to HTTP(S)");
        };
    }
}
