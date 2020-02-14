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

#include "openssl/evp.h"
#include "openssl/pem.h"
#include "openssl/err.h"
#include "openssl/ec.h"

#include <cryptopp/eccrypto.h>
#include <cryptopp/osrng.h>
#include <cryptopp/sha.h>
#include <cryptopp/oids.h>
#include <cryptopp/hex.h>

#include "stubclient.h"

#include "SkaleCommon.h"
#include "Log.h"
#include "thirdparty/json.hpp"
#include "messages/NetworkMessage.h"
#include "chains/Schain.h"
#include "SHAHash.h"
#include "ConsensusBLSSigShare.h"
#include "ConsensusBLSSignature.h"
#include "MockupSigShare.h"
#include "ConsensusSigShareSet.h"
#include "MockupSignature.h"
#include "MockupSigShareSet.h"

#include "node/Node.h"
#include "monitoring/LivelinessMonitor.h"
#include "datastructures/BlockProposal.h"
#include "bls/BLSPrivateKeyShare.h"

#include "CryptoManager.h"


CryptoManager::CryptoManager(Schain &_sChain) : sChain(&_sChain) {
    CHECK_ARGUMENT(sChain != nullptr);
    static string empty = "";
    sgxIP = _sChain.getNode()->getParamString("sgxIP", empty);

    if (sgxIP->length() == 0)
        sgxIP = nullptr;
    else {
        sgxEnabled = true;
        auto node = _sChain.getNode();
        sgxSSLKeyFileFullPath = node->getParamString("sgxKeyFileFullPath", empty);
        sgxSSLCertFileFullPath = node->getParamString("sgxCertFileFullPath", empty);
        sgxECDSAKeyName = node->getParamString("sgxECDSAKeyName", empty);
        if (sgxECDSAKeyName->length() == 0) {
            auto keyName = std::getenv("sgxECDSAKeyName." + _sChain.getSchainIndex());
            ASSERT(keyName != nullptr);
            sgxECDSAKeyName = make_shared<string>(keyName);
        }

        sgxECDSAPublicKey = node->getParamString("sgxECDSAPublicKey", empty);
        if (sgxECDSAPublicKey->length() == 0) {
            auto publicKey = std::getenv("sgxECDSAPublicKey." + _sChain.getSchainIndex());
            ASSERT(publicKey != nullptr);
            sgxECDSAPublicKey = make_shared<string>(publicKey);
        }


        ASSERT(sgxSSLKeyFileFullPath->length() > 0);
        ASSERT(sgxSSLCertFileFullPath->length() > 0);

        jsonrpc::HttpClient::setKeyFileFullPath("/tmp/key");
        jsonrpc::HttpClient::setCertFileFullPath("/tmp/cert");
        jsonrpc::HttpClient::setSslClientPort(SGX_SSL_PORT);

        jsonrpc::HttpClient httpClient("https://" + *sgxIP + ":1026");
        sgxClient = make_shared<StubClient>(httpClient, jsonrpc::JSONRPC_CLIENT_V2);
    }

    totalSigners = sChain->getTotalSigners();
    requiredSigners = sChain->getRequiredSigners();
}

Schain *CryptoManager::getSchain() const {
    return sChain;
}

ptr<string> CryptoManager::signECDSA(ptr<SHAHash> _hash) {
    if (sgxEnabled) {
        auto result = sgxClient->ecdsaSignMessageHash(16, *sgxECDSAKeyName, *_hash->toHex());
        auto status = result["status"].asInt64();
        CHECK_STATE(status == 0);
        string r = result["signature_r"].asString();
        string s = result["signature_r"].asString();
        string v = result["signature_v"].asString();

        return make_shared<string>(r.substr(2) + ":" + s.substr(2) + ":" + v);

    } else {
        return _hash->toHex();
    }
}

bool CryptoManager::verifyECDSA(ptr<SHAHash> _hash, ptr<string> _sig) {
    CHECK_ARGUMENT(_hash != nullptr)
    CHECK_ARGUMENT(_sig != nullptr)
    return *_sig == *(_hash->toHex());
}


ptr<ThresholdSigShare> CryptoManager::signDAProofSigShare(ptr<BlockProposal> _p) {
    CHECK_ARGUMENT(_p != nullptr);
    return signSigShare(_p->getHash(), _p->getBlockID());
}

ptr<ThresholdSigShare> CryptoManager::signBinaryConsensusSigShare(ptr<SHAHash> _hash, block_id _blockId) {
    return signSigShare(_hash, _blockId);
}

ptr<ThresholdSigShare> CryptoManager::signBlockSigShare(ptr<SHAHash> _hash, block_id _blockId) {
    return signSigShare(_hash, _blockId);
}

ptr<ThresholdSigShare> CryptoManager::signSigShare(ptr<SHAHash> _hash, block_id _blockId) {


    MONITOR(__CLASS_NAME__, __FUNCTION__)

    if (getSchain()->getNode()->isBlsEnabled()) {

        auto hash = make_shared<std::array<uint8_t, 32>>();

        memcpy(hash->data(), _hash->data(), 32);

        auto blsShare = sChain->getNode()->getBlsPrivateKey()->sign(hash, (uint64_t) sChain->getSchainIndex());

        return make_shared<ConsensusBLSSigShare>(blsShare, sChain->getSchainID(), _blockId);


    } else {
        auto sigShare = _hash->toHex();
        return make_shared<MockupSigShare>(sigShare, sChain->getSchainID(), _blockId,
                                           sChain->getSchainIndex(),
                                           sChain->getTotalSigners(),
                                           sChain->getRequiredSigners());
    }
}

ptr<ThresholdSigShareSet>
CryptoManager::createSigShareSet(block_id _blockId) {
    if (getSchain()->getNode()->isBlsEnabled()) {
        return make_shared<ConsensusSigShareSet>(_blockId, totalSigners, requiredSigners);
    } else {
        return make_shared<MockupSigShareSet>(_blockId, totalSigners, requiredSigners);
    }
}


ptr<ThresholdSigShare>
CryptoManager::createSigShare(ptr<string> _sigShare, schain_id _schainID, block_id _blockID,
                              schain_index _signerIndex) {
    if (getSchain()->getNode()->isBlsEnabled()) {
        return make_shared<ConsensusBLSSigShare>(_sigShare, _schainID, _blockID, _signerIndex,
                                                 totalSigners, requiredSigners);
    } else {
        return make_shared<MockupSigShare>(_sigShare, _schainID, _blockID, _signerIndex,
                                           totalSigners, requiredSigners);
    }
}

void CryptoManager::signProposalECDSA(BlockProposal *_proposal) {
    CHECK_ARGUMENT(_proposal != nullptr);
    auto signature = signECDSA(_proposal->getHash());
    CHECK_STATE(signature != nullptr);
    _proposal->addSignature(signature);
}

ptr<string> CryptoManager::signNetworkMsg(NetworkMessage &_msg) {
    auto signature = signECDSA(_msg.getHash());
    CHECK_STATE(signature != nullptr);
    return signature;
}

bool CryptoManager::verifyNetworkMsg(NetworkMessage &_msg) {
    auto sig = _msg.getECDSASig();
    auto hash = _msg.getHash();

    if (!verifyECDSA(hash, sig)) {
        LOG(warn, "ECDSA sig did not verify");
        return false;
    }

    return true;
}

bool CryptoManager::verifyProposalECDSA(ptr<BlockProposal> _proposal, ptr<string> _hashStr, ptr<string> _signature) {
    CHECK_ARGUMENT(_proposal != nullptr);
    CHECK_ARGUMENT(_hashStr != nullptr)
    CHECK_ARGUMENT(_signature != nullptr)
    auto hash = _proposal->getHash();


    if (*hash->toHex() != *_hashStr) {
        LOG(warn, "Incorrect proposal hash");
        return false;
    }

    if (!verifyECDSA(hash, _signature)) {
        LOG(warn, "ECDSA sig did not verify");
        return false;
    }
    return true;
}

ptr<ThresholdSignature>
CryptoManager::verifyThresholdSig(ptr<SHAHash> _hash, ptr<string> _signature, block_id _blockId) {
    MONITOR(__CLASS_NAME__, __FUNCTION__)


    if (getSchain()->getNode()->isBlsEnabled()) {

        auto hash = make_shared<std::array<uint8_t, 32>>();

        memcpy(hash->data(), _hash->data(), 32);


        auto sig = make_shared<ConsensusBLSSignature>(_signature, _blockId, requiredSigners, totalSigners);

        if (!sChain->getNode()->getBlsPublicKey()->VerifySig(hash,
                                                             sig->getBlsSig(), requiredSigners, totalSigners)) {
            BOOST_THROW_EXCEPTION(InvalidArgumentException("BLS Signature did not verify",
                                                           __CLASS_NAME__));
        }
        return sig;
    } else {

        auto sig = make_shared<MockupSignature>(_signature, _blockId, requiredSigners, totalSigners);

        if (*sig->toString() != *_hash->toHex()) {
            BOOST_THROW_EXCEPTION(InvalidArgumentException("Mockup threshold signature did not verify",
                                                           __CLASS_NAME__));
        }
        return sig;
    }
}

using namespace CryptoPP;

ptr<void> CryptoManager::decodeSGXPublicKey(ptr<string> _keyHex) {

    HexDecoder decoder;
    CHECK_STATE(decoder.Put((unsigned char *) _keyHex->data(), _keyHex->size()) == 0);
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

pair<ptr<string>, ptr<string>> CryptoManager::generateSGXECDSAKey(ptr<StubClient> _c) {

    auto result = _c->generateECDSAKey();

    auto status = result["status"].asInt64();
    CHECK_STATE(status == 0);
    cerr << result << endl;
    auto keyName = make_shared<string>(result["keyName"].asString());
    auto publicKey = make_shared<string>(result["publicKey"].asString());

    CHECK_STATE(keyName->size() > 10);
    CHECK_STATE(publicKey->size() > 10);
    CHECK_STATE(keyName->find("NEK") != -1);
    cerr << *keyName << endl;
    cerr << *publicKey << endl;

    return {keyName, publicKey};
}