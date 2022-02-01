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
    @date 2019-
*/


#include <boost/asio.hpp>

#include "openssl/bio.h"


#include <cryptopp/eccrypto.h>
#include <cryptopp/hex.h>
#include <cryptopp/oids.h>

#include <cryptopp/sha.h>
#include <cryptopp/sha3.h>

#include "JsonStubClient.h"


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
// #include "datastructures/BlockProposal.h"
#include "datastructures/CommittedBlock.h"
#include "monitoring/LivelinessMonitor.h"
#include "node/Node.h"
#include "node/NodeInfo.h"

#include "blockdecrypt/client/BlockDecryptDownloader.h"

#include "json/JSONFactory.h"
#include "network/Utils.h"
#include "datastructures/TransactionList.h"
#include "datastructures/Transaction.h"
#include "node/ConsensusInterface.h"
#include "utils/Time.h"
#include "OpenSSLECDSAKey.h"
#include "OpenSSLEdDSAKey.h"
#include "datastructures/BlockEncryptedArguments.h"

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

pair<ptr<BLSPublicKey>, ptr<BLSPublicKey >> CryptoManager::getSgxBlsPublicKey(uint64_t _timestamp) {
    if (_timestamp == uint64_t(-1) || previousBlsPublicKeys->size() < 2) {
        CHECK_STATE(sgxBLSPublicKey)
        return {sgxBLSPublicKey, nullptr};
    } else {
        // second key is used when the sig corresponds
        // to the last block before node rotation!
        // in this case we use the key for the group before

        // could not return iterator to end()
        // because finish ts for the current group equals uint64_t(-1)
        auto it = previousBlsPublicKeys->upper_bound(_timestamp);

        if (it == previousBlsPublicKeys->begin()) {
            // if begin() then no previous groups for this key
            return {(*it).second, nullptr};
        }

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


    Utils::cArrayFromHex(TE_MAGIC_START, teMagicStart.data(), TE_MAGIC_SIZE);
    Utils::cArrayFromHex(TE_MAGIC_END, teMagicEnd.data(), TE_MAGIC_SIZE);



    CHECK_ARGUMENT(_totalSigners >= _requiredSigners);
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

    CHECK_ARGUMENT(totalSigners >= requiredSigners);

    isSGXEnabled = _sChain.getNode()->isSgxEnabled();

    if (isSGXEnabled) {
        auto node = _sChain.getNode();
        sgxURL = node->getSgxUrl();
        if (sgxURL.back() == '/') {
            sgxURL = sgxURL.substr(0, sgxURL.size() - 1);
        }

        sgxSSLCertFileFullPath = node->getSgxSslCertFileFullPath();
        sgxSSLKeyFileFullPath = node->getSgxSslKeyFileFullPath();
        sgxECDSAKeyName = node->getEcdsaKeyName();
        sgxECDSAPublicKeys = node->getEcdsaPublicKeys();
        sgxBlsKeyName = node->getBlsKeyName();
        sgxBLSPublicKeys = node->getBlsPublicKeys();
        sgxBLSPublicKey = node->getBlsPublicKey();
        previousBlsPublicKeys = node->getPreviousBLSPublicKeys();
        tie(sgxDomainName, sgxPort) = parseSGXDomainAndPort(sgxURL);

        CHECK_STATE(sgxURL != "");
        CHECK_STATE(sgxECDSAKeyName != "");
        CHECK_STATE(sgxECDSAPublicKeys);
        CHECK_STATE(sgxBlsKeyName != "");
        CHECK_STATE(sgxBLSPublicKeys);
        CHECK_STATE(sgxBLSPublicKey);

        CHECK_STATE(JSONFactory::splitString(sgxBlsKeyName)->size() == 7);
        CHECK_STATE(JSONFactory::splitString(sgxECDSAKeyName)->size() == 2);

        isHTTPSEnabled = sgxURL.find("https:/") != string::npos;

        isSSLCertEnabled =
                (!sgxSSLKeyFileFullPath.empty()) && (!sgxSSLCertFileFullPath.empty());

        initSGXClient();

        for (uint64_t i = 0; i < (uint64_t) getSchain()->getNodeCount(); i++) {
            auto nodeId = getSchain()->getNode()->getNodeInfoByIndex(i + 1)->getNodeID();
            ecdsaPublicKeyMap[(uint64_t) nodeId] = sgxECDSAPublicKeys->at(i);
            blsPublicKeyMap[(uint64_t) nodeId] = sgxBLSPublicKeys->at(i);

            if (nodeId == getSchain()->getThisNodeInfo()->getNodeID()) {
                auto publicKey = getSGXEcdsaPublicKey(sgxECDSAKeyName, getSgxClient());
                if (publicKey != sgxECDSAPublicKeys->at(i)) {
                    BOOST_THROW_EXCEPTION(InvalidStateException(
                                                  "Misconfiguration. \n Configured ECDSA public key for this node \n" +
                                                  sgxECDSAPublicKeys->at(i) +
                                                  " \n is not equal to the public key for \n " + sgxECDSAKeyName +
                                                  "\n  on the SGX server: \n" + publicKey,
                                                          __CLASS_NAME__ ));
                };
            }
        }
    }
}

void CryptoManager::setSGXKeyAndCert(
        string &_keyFullPath, string &_certFullPath, uint64_t _sgxPort) {
    jsonrpc::HttpClient::setKeyFileFullPath(_keyFullPath);
    jsonrpc::HttpClient::setCertFileFullPath(_certFullPath);
    jsonrpc::HttpClient::setSslClientPort(_sgxPort);
}

Schain *CryptoManager::getSchain() const {
    CHECK_STATE(sChain)
    return sChain;
}


#define ECDSA_SKEY_LEN 65
#define ECDSA_SKEY_BASE 16

#define BUF_SIZE 1024
#define PUB_KEY_SIZE 64


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


tuple<string, string, string> CryptoManager::sessionSignECDSA(
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

BLAKE3Hash CryptoManager::calculatePublicKeyHash(
        const string publicKey, block_id _blockID) {
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

    ret = zmqClient->ecdsaSignMessageHash(16, _keyName, _hash.toHex(), false);

    return ret;
}


bool CryptoManager::verifyECDSA(
        BLAKE3Hash &_hash, const string &_sig, const string &_publicKey) {
    auto key = OpenSSLECDSAKey::importSGXPubKey(_publicKey);

    return key->verifySGXSig(_sig, (const char *) _hash.data());
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

    auto hashStr = hashForOracle(_text);

    if (isSGXEnabled) {
        CHECK_STATE(sgxECDSAKeyName != "")
        result = zmqClient->ecdsaSignMessageHash(16,
                                                 sgxECDSAKeyName,
                                                 hashStr, true);
    } else {
        result = hashStr;
    }


    return result;

}

string CryptoManager::hashForOracle(string &_text) {

    _text = "age";

    try {
        CryptoPP::SHA3_256 hash;
        string digest;


        hash.Update((const CryptoPP::byte *) _text.data(), _text.size());
        digest.resize(hash.DigestSize());
        hash.Final((CryptoPP::byte *) &digest[0]);

        return Utils::carray2Hex((const uint8_t *) digest.data(), HASH_LEN);
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}

tuple<string, string, string> CryptoManager::sessionSign(
        BLAKE3Hash &_hash, block_id _blockId) {

    if (isSGXEnabled) {
        string signature = "";
        string pubKey = "";
        string pkSig = "";

        tie(signature, pubKey, pkSig) = sessionSignECDSA(_hash, _blockId);
        CHECK_STATE(signature != "");
        CHECK_STATE(pubKey != "");
        CHECK_STATE(pkSig != "");
        return {signature, pubKey, pkSig};
    } else {
        return {_hash.toHex(), "", ""};
    }
}


bool CryptoManager::sessionVerifyEdDSASig(
        BLAKE3Hash &_hash, const string &_sig, const string &_publicKey) {

    CHECK_ARGUMENT(_sig != "")

    if (isSGXEnabled) {
        auto pkey = OpenSSLEdDSAKey::importPubKey(_publicKey);
        return pkey->verifySig(_sig, (const char *) _hash.data());
    } else {
        // mockup - used for testing
        if (_sig.find(":") != string::npos) {
            LOG(critical,
                "Misconfiguration: this node is in mockup signature mode,"
                "but other node sent a real signature ");
            exit(-1);
        }

        return _sig == _hash.toHex();
    }
}


bool CryptoManager::verifyECDSASig(
        BLAKE3Hash &_hash, const string &_sig, node_id _nodeId) {
    CHECK_ARGUMENT(_sig != "")

    if (isSGXEnabled) {
        string pubKey;

        {
            LOCK(ecdsaPublicKeyMapLock)

            if (ecdsaPublicKeyMap.count((uint64_t) _nodeId) == 0) {
                // if there is no key report the signature as failed
                return false;
            }

            pubKey = ecdsaPublicKeyMap.at((uint64_t) _nodeId);
        }

        CHECK_STATE(pubKey != "");
        auto result = verifyECDSA(_hash, _sig, pubKey);

        return result;


    } else {
        // mockup - used for testing
        if (_sig.find(":") != string::npos) {
            LOG(critical,
                "Misconfiguration: this node is in mockup signature mode,"
                "but other node sent a real signature ");
            exit(-1);
        }

        return _sig == (_hash.toHex());
    }
}


tuple<ptr<ThresholdSigShare>, string, string, string> CryptoManager::signDAProof(
        const ptr<BlockProposal> &_p) {
    CHECK_ARGUMENT(_p);


    auto h = _p->getHash();

    auto sigShare = signDAProofSigShare(h, _p->getBlockID(), false);
    CHECK_STATE(sigShare);

    auto combinedHash = BLAKE3Hash::merkleTreeMerge(_p->getHash(), sigShare->computeHash());
    auto[ecdsaSig, pubKey, pubKeySig] = sessionSign(combinedHash, _p->getBlockID());
    return {sigShare, ecdsaSig, pubKey, pubKeySig};
}


ptr<ThresholdSigShare> CryptoManager::signBinaryConsensusSigShare(
        BLAKE3Hash &_hash, block_id _blockId, uint64_t _round) {
    auto result = signSigShare(_hash, _blockId, ((uint64_t) _round) < COMMON_COIN_ROUND);
    CHECK_STATE(result);
    return result;
}

ptr<ThresholdSigShare> CryptoManager::signBlockSigShare(
        BLAKE3Hash &_hash, block_id _blockId) {
    auto result = signSigShare(_hash, _blockId, false);
    CHECK_STATE(result);
    return result;
}

void CryptoManager::verifyBlockSig(
        ptr<ThresholdSignature> _signature, BLAKE3Hash &_hash, const TimeStamp &_ts) {
    CHECK_STATE(_signature);
    verifyThresholdSig(_signature, _hash, false, _ts);
}


void CryptoManager::verifyBlockSig(
        string &_sigStr, block_id _blockId, BLAKE3Hash &_hash, const TimeStamp &_ts) {
    if (getSchain()->getNode()->isSgxEnabled()) {

        auto _signature = make_shared<ConsensusBLSSignature>(_sigStr, _blockId,
                                                             totalSigners, requiredSigners);

        verifyBlockSig(_signature, _hash, _ts);
    }
}


ptr<ThresholdSigShare> CryptoManager::signDAProofSigShare(
        BLAKE3Hash &_hash, block_id _blockId, bool _forceMockup) {
    MONITOR(__CLASS_NAME__, __FUNCTION__)

    if (getSchain()->getNode()->isSgxEnabled() && !_forceMockup) {
        auto&&[sig, publicKey, pkSig] = this->sessionSignECDSA(_hash, _blockId);

        auto share = to_string((uint64_t) getSchain()->getSchainIndex()) + ";" + sig + ";" +
                     publicKey + ";" + pkSig;

        return make_shared<ConsensusEdDSASigShare>(share, sChain->getSchainID(), _blockId,
                                                   sChain->getSchainIndex(), totalSigners, requiredSigners);

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

    if (getSchain()->getNode()->isSgxEnabled() && !_forceMockup) {
        auto sShare = dynamic_pointer_cast<ConsensusEdDSASigShare>(_sigShare);

        CHECK_STATE(sShare);

        sShare->verify(*this, _schainIndex, _hash, _nodeId);

        return;

    } else {
        return;
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


        ret = zmqClient->blsSignMessageHash(
                getSgxBlsKeyName(), _hash.toHex(), requiredSigners, totalSigners,
                false);


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

void CryptoManager::verifyThresholdSig(
        ptr<ThresholdSignature> _signature, BLAKE3Hash &_hash, bool _forceMockup, const TimeStamp &_ts) {

    CHECK_STATE(_signature);

    MONITOR(__CLASS_NAME__, __FUNCTION__)

    if (getSchain()->getNode()->isSgxEnabled() && !_forceMockup) {

        auto blsSig = dynamic_pointer_cast<ConsensusBLSSignature>(_signature);

        CHECK_STATE(blsSig);

        auto blsKeys = getSgxBlsPublicKey(_ts.getS());

        auto libBlsSig = blsSig->getBlsSig();


        if (!blsKeys.first->VerifySig(make_shared<array<uint8_t, HASH_LEN>>(_hash.getHash()), libBlsSig)) {
            // second key is used when the sig corresponds
            // to the last block before node rotation!
            // in this case we use the key for the group before
            CHECK_STATE(blsKeys.second);
            CHECK_STATE(blsKeys.second->VerifySig(
                    make_shared<array<uint8_t, HASH_LEN>>(_hash.getHash()),
                    libBlsSig));
        }

    } else {
        // mockups sigs are not verified
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
        return make_shared<ConsensusEdDSASigShareSet>(_blockId, totalSigners, requiredSigners);
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
    CHECK_STATE(totalSigners >= requiredSigners);


    if (getSchain()->getNode()->isSgxEnabled() && !_forceMockup) {
        auto result = make_shared<ConsensusEdDSASigShare>(
                _sigShare, _schainID, _blockID, _signerIndex, totalSigners, requiredSigners);

        return result;

    } else {
        return make_shared<MockupSigShare>(
                _sigShare, _schainID, _blockID, _signerIndex, totalSigners, requiredSigners);
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
    auto&&[signature, publicKey, pkSig] = sessionSign(h, _msg.getBlockId());
    CHECK_STATE(signature != "");
    return {signature, publicKey, pkSig};
}


bool CryptoManager::verifyNetworkMsg(NetworkMessage &_msg) {
    auto sig = _msg.getECDSASig();
    auto hash = _msg.getHash();
    auto publicKey = _msg.getPublicKey();
    auto pkSig = _msg.getPkSig();
    auto blockId = _msg.getBlockID();
    auto nodeId = _msg.getSrcNodeID();
    return sessionVerifySigAndKey(hash, sig, publicKey, pkSig, blockId, nodeId);
}


bool CryptoManager::sessionVerifySigAndKey(BLAKE3Hash &_hash, const string &_sig,
                                           const string &_publicKey, const string &pkSig, block_id _blockID,
                                           node_id _nodeId) {
    MONITOR(__CLASS_NAME__, __FUNCTION__);


    CHECK_STATE(!_sig.empty());


    {
        LOCK(publicSessionKeysLock)

        if (auto result = sessionPublicKeys.getIfExists(pkSig); result.has_value()) {
            auto publicKey2 = any_cast<string>(result);
            if (publicKey2 != _publicKey)
                return false;
        } else {
            if (isSGXEnabled) {
                auto pkeyHash = calculatePublicKeyHash(_publicKey, _blockID);
                if (!verifyECDSASig(pkeyHash, pkSig, _nodeId)) {
                    LOG(warn, "PubKey ECDSA sig did not verify");
                    return false;
                }
                sessionPublicKeys.put(pkSig, _publicKey);
            }
        }
    }

    if (!sessionVerifyEdDSASig(_hash, _sig, _publicKey)) {
        LOG(warn, "ECDSA sig did not verify");
        return false;
    }


    return true;
}

bool CryptoManager::verifyProposalECDSA(
        const ptr<BlockProposal> &_proposal, const string &_hashStr, const string &_signature) {
    CHECK_ARGUMENT(_proposal);
    CHECK_ARGUMENT(_hashStr != "")
    CHECK_ARGUMENT(_signature != "")

    // default proposal is not signed using ECDSA
    if (_proposal->getProposerIndex() == 0) {
        return true;
    }

    auto hash = _proposal->getHash();

    if (hash.toHex() != _hashStr) {
        LOG(warn, "Incorrect proposal hash");
        return false;
    }

    if (!verifyECDSASig(hash, _signature, _proposal->getProposerNodeID())) {
        LOG(warn, "ECDSA sig did not verify");
        return false;
    }
    return true;
}


ptr<ThresholdSignature> CryptoManager::verifyDAProofThresholdSig(
        BLAKE3Hash &_hash, const string &_signature, block_id _blockId) {
    MONITOR(__CLASS_NAME__, __FUNCTION__)


    CHECK_ARGUMENT(!_signature.empty());

    if (getSchain()->getNode()->isSgxEnabled()) {
        auto sig = make_shared<ConsensusEdDSASignature>(
                _signature, _blockId, totalSigners, requiredSigners);

        return sig;

    } else {
        auto sig =
                make_shared<MockupSignature>(_signature, _blockId, requiredSigners, totalSigners);

        if (sig->toString() != _hash.toHex()) {
            BOOST_THROW_EXCEPTION(InvalidArgumentException(
                                          "Mockup threshold signature did not verify", __CLASS_NAME__ ));
        }
        return sig;
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

ptr<map<uint64_t, string>> CryptoManager::decryptArgKeys(ptr<BlockProposal> _proposal) {
    CHECK_STATE(_proposal);

    try {
        auto encryptedArgs = _proposal->getEncryptedArguments(*getSchain());
    } catch (exception& e) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

    return make_shared<map<uint64_t, string>>();
}



ptr<BlockDecryptedArguments> CryptoManager::decryptArgs(ptr<BlockProposal> _block) {

    CHECK_STATE(_block);

    auto args = make_shared<BlockEncryptedArguments>(_block, getSchain()->getNode()->getEncryptedTransactionAnalyzer());

    auto agent = make_unique<BlockDecryptDownloader>(getSchain(), _block->getBlockID());

    {
        const string msg = "Decryption download:" + to_string(
                (uint64_t) _block->getBlockID()
        );

        MONITOR(__CLASS_NAME__, msg.c_str());
        // This will complete successfully also if block arrives through catchup
        auto decryptions = agent->downloadDecryptions();
    }

    return nullptr;
}

const array<uint8_t, TE_MAGIC_SIZE> &CryptoManager::getTeMagicStart() const {
    return teMagicStart;
}

const array<uint8_t, TE_MAGIC_SIZE> &CryptoManager::getTeMagicEnd() const {
    return teMagicEnd;
}

const AutoSeededRandomPool &CryptoManager::getPrng() const {
    return prng;
}

string CryptoManager::teEncryptAESKey(ptr<vector<uint8_t>> _aesKey) {
    CHECK_STATE(_aesKey)
    CHECK_STATE(_aesKey->size() == CryptoPP::AES::DEFAULT_KEYLENGTH);

    if (!isSGXEnabled) {
        // mockup - dont encrypt
        return Utils::carray2Hex(_aesKey->data(), _aesKey->size());
    } else {
        // Encrypt key using TE public key, so it can be decrypted later
        return "";
    }
}



