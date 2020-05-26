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

#include "JsonStubClient.h"


#include <jsonrpccpp/client/connectors/httpclient.h>




#include "SkaleCommon.h"
#include "SkaleLog.h"

#include <gmp.h>
#include "secure_enclave/Verify.h"

#include "ConsensusBLSSigShare.h"
#include "ConsensusBLSSignature.h"
#include "ConsensusSigShareSet.h"
#include "MockupSigShare.h"
#include "MockupSigShareSet.h"
#include "MockupSignature.h"
#include "SHAHash.h"

#include "chains/Schain.h"
#include "messages/NetworkMessage.h"
#include "thirdparty/json.hpp"

#include "node/Node.h"
#include "monitoring/LivelinessMonitor.h"
#include "datastructures/BlockProposal.h"
#include "bls/BLSPrivateKeyShare.h"

#include "ECDSAVerify.h"
#include "CryptoManager.h"


CryptoManager::CryptoManager(Schain &_sChain) : sChain(&_sChain) {
    CHECK_ARGUMENT(sChain != nullptr);

    ecdsaVerify = make_shared<ECDSAVerify>();

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


        for (int i = 1; i <= _sChain.getNodeCount(); i++) {

            string paramName = "sgxECDSAPublicKey." + to_string(i);

            sgxECDSAPublicKeys.at(i) = node->getParamString(paramName, empty);
            if (sgxECDSAPublicKeys.at(i)->length() == 0) {
                auto publicKey = std::getenv(paramName.c_str());
                ASSERT(publicKey != nullptr);
                sgxECDSAPublicKeys.at(i) = make_shared<string>(publicKey);
            }
        }


        ASSERT(sgxSSLKeyFileFullPath->length() > 0);
        ASSERT(sgxSSLCertFileFullPath->length() > 0);

        setSGXKeyAndCert(*sgxSSLKeyFileFullPath, *sgxSSLCertFileFullPath);

        jsonrpc::HttpClient httpClient("https://" + *sgxIP + ":1026");
        sgxClient = make_shared<StubClient>(httpClient, jsonrpc::JSONRPC_CLIENT_V2);
    }

    totalSigners = sChain->getTotalSigners();
    requiredSigners = sChain->getRequiredSigners();
}

void CryptoManager::setSGXKeyAndCert(string &_keyFullPath, string &_certFullPath) {
    jsonrpc::HttpClient::setKeyFileFullPath(_keyFullPath);
    jsonrpc::HttpClient::setCertFileFullPath(_certFullPath);
    jsonrpc::HttpClient::setSslClientPort(SGX_SSL_PORT);
}

Schain *CryptoManager::getSchain() const {
    return sChain;
}


ptr<string> CryptoManager::sgxSignECDSA(ptr<SHAHash> _hash, string &_keyName, ptr<StubClient> _sgxClient) {
    CHECK_ARGUMENT(_sgxClient);
    auto result = _sgxClient->ecdsaSignMessageHash(16, _keyName, *_hash->toHex());
    auto status = result["status"].asInt64();
    CHECK_STATE(status == 0);
    string r = result["signature_r"].asString();
    string s = result["signature_r"].asString();
    string v = result["signature_v"].asString();

    return make_shared<string>(v + ":" + r.substr(2) + ":" + s.substr(2));

}

bool CryptoManager::sgxVerifyECDSA(ptr<SHAHash> _hash, ptr<string> _publicKey, ptr<string> _sig) {

    CHECK_ARGUMENT(_hash);
    CHECK_ARGUMENT(_sig);
    CHECK_ARGUMENT(_publicKey)

    bool result = false;

    signature sig;
    domain_parameters curve;
    mpz_t msgMpz;
    point publicKey;

    try {

        sig = signature_init();

        auto firstColumn = _sig->find( ":" );

        if ( firstColumn == string::npos || firstColumn == _sig->length() - 1 ) {
            LOG( warn, "Misfomatted signature" );
            throw exception();
        }

        auto secondColumn = _sig->find( ":", firstColumn + 1 );

        if ( secondColumn == string::npos || secondColumn == _sig->length() - 1 ) {
            LOG( warn, "Misformatted signature" );
            throw exception();
        }

        auto r = _sig->substr( firstColumn + 1, secondColumn - firstColumn - 1 );
        auto s = _sig->substr( secondColumn + 1, _sig->length() - secondColumn - 1 );


        if ( _publicKey->size() != 128 ) {
            LOG( warn, "ECDSA verify fail: _publicKey->size() != 128" );
            throw exception();
        }

        auto pubKeyR = _publicKey->substr( 0, 64 );
        auto pubKeyS = _publicKey->substr( 64, 128 );
        curve  = domain_parameters_init();
        domain_parameters_load_curve( curve, secp256k1 );
        publicKey = point_init();



        mpz_init( msgMpz );

        auto hashHex = _hash->toHex();

        if ( mpz_set_str( msgMpz, hashHex->c_str(), 16 ) == -1 ) {
            LOG( warn, "invalid message hash " + *hashHex );
            throw exception();
        }

        if ( signature_set_str( sig, r.c_str(), s.c_str(), 16 ) != 0 ) {
            LOG( warn, "Misformatted ECDSA sig " );
        }
        point_set_hex( publicKey, pubKeyR.c_str(), pubKeyS.c_str() );
        if ( !signature_verify( msgMpz, sig, publicKey, curve ) ) {
            LOG( warn, "ECDSA sig not verified" );
            throw exception();
        }

        result = true;
    } catch (...) {

    }

    mpz_clear(msgMpz);
    domain_parameters_clear(curve);
    point_clear(publicKey);
    signature_free(sig);

    return result;


}

ptr<string> CryptoManager::signECDSA(ptr<SHAHash> _hash) {
    CHECK_ARGUMENT(_hash);
    if (sgxEnabled) {
        return sgxSignECDSA(_hash, *sgxECDSAKeyName, sgxClient);
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
    CHECK_STATE(keyName->find("NEK") != string::npos);
    cerr << *keyName << endl;
    cerr << *publicKey << endl;

    return {keyName, publicKey};
}


void CryptoManager::generateSSLClientCertAndKey(string &_fullPathToDir) {

    const std::string VALID_CHARS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<int> distribution(0, VALID_CHARS.size() - 1);
    std::string random_string;
    std::generate_n(std::back_inserter(random_string), 10, [&]() {
        return VALID_CHARS[distribution(generator)];
    });
    system(("/usr/bin/openssl req -new -sha256 -nodes -out " + _fullPathToDir + "/csr  -newkey rsa:2048 -keyout "
            + _fullPathToDir + "/key -subj /CN=" + random_string).data());
    string str, csr;
    ifstream file;
    file.open(_fullPathToDir + "/csr");
    while (getline(file, str)) {
        csr.append(str);
        csr.append("\n");
    }

    jsonrpc::HttpClient client("http://localhost:1027");
    StubClient c(client, jsonrpc::JSONRPC_CLIENT_V2);

    auto result = c.SignCertificate(csr);
    int64_t status = result["status"].asInt64();
    CHECK_STATE(status == 0);
    string certHash = result["hash"].asString();
    result = c.GetCertificate(certHash);
    status = result["status"].asInt64();
    CHECK_STATE(status == 0);
    string signedCert = result["cert"].asString();
    ofstream outFile;
    outFile.open(_fullPathToDir + "/cert");
    outFile << signedCert;

}

CryptoManager::CryptoManager(uint64_t totalSigners, uint64_t requiredSigners, const ptr<string> &sgxIp,
                             const ptr<string> &sgxSslKeyFileFullPath, const ptr<string> &sgxSslCertFileFullPath,
                             const ptr<string> &sgxEcdsaKeyName, const vector<ptr<string>> &sgxEcdsaPublicKeys)
        : totalSigners(totalSigners), requiredSigners(requiredSigners), sgxIP(sgxIp),
          sgxSSLKeyFileFullPath(sgxSslKeyFileFullPath), sgxSSLCertFileFullPath(sgxSslCertFileFullPath),
          sgxECDSAKeyName(sgxEcdsaKeyName), sgxECDSAPublicKeys(sgxEcdsaPublicKeys) {
    ecdsaVerify = make_shared<ECDSAVerify>();
    this->sgxEnabled = sgxIp != nullptr;
}









