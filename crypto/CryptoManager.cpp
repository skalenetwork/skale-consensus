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


#include "../SkaleCommon.h"
#include "../Log.h"
#include "../thirdparty/json.hpp"
#include "../chains/Schain.h"
#include "../exceptions/NetworkProtocolException.h"
#include "SHAHash.h"
#include "ConsensusBLSSigShare.h"
#include "ConsensusBLSSignature.h"
#include "MockupSigShare.h"
#include "ConsensusSigShareSet.h"
#include "MockupSignature.h"
#include "MockupSigShareSet.h"
#include "../node/Node.h"
#include "../monitoring/LivelinessMonitor.h"
#include "../datastructures/BlockProposal.h"
#include "bls/BLSPrivateKeyShare.h"


#include "CryptoManager.h"


void CryptoManager::init() {

}


CryptoManager::CryptoManager() : sChain(nullptr) {
    init();
}


CryptoManager::CryptoManager(Schain &_sChain) : sChain(&_sChain) {
    CHECK_ARGUMENT(sChain != nullptr);
    init();

}

Schain *CryptoManager::getSchain() const {
    return sChain;
}

ptr<string> CryptoManager::signECDSA(ptr<SHAHash> _hash) {
    return _hash->toHex();
}


bool CryptoManager::verifyECDSA(ptr<SHAHash> _hash,
        ptr<string> _sig) {
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

        return make_shared<ConsensusBLSSigShare>(blsShare, sChain->getSchainID(), _blockId,
                                                 sChain->getNode()->getNodeID());

    } else {
        auto sigShare = _hash->toHex();
        return make_shared<MockupSigShare>(sigShare, sChain->getSchainID(), _blockId,
                                           sChain->getNode()->getNodeID(),
                                           sChain->getSchainIndex(),
                                           sChain->getTotalSigners(),
                                           sChain->getRequiredSigners());
    }
}

ptr<ThresholdSigShareSet>
CryptoManager::createSigShareSet(block_id _blockId, size_t _totalSigners, size_t _requiredSigners) {
    if (getSchain()->getNode()->isBlsEnabled()) {
        return make_shared<ConsensusSigShareSet>(_blockId, _totalSigners, _requiredSigners);
    } else {
        return make_shared<MockupSigShareSet>(_blockId, _totalSigners, _requiredSigners);
    }
}




ptr<ThresholdSigShare>
CryptoManager::createSigShare(ptr<string> _sigShare, schain_id _schainID, block_id _blockID, node_id _signerNodeID,
                              schain_index _signerIndex, size_t _totalSigners, size_t _requiredSigners) {
    if (getSchain()->getNode()->isBlsEnabled()) {
        return make_shared<ConsensusBLSSigShare>(_sigShare, _schainID, _blockID, _signerNodeID, _signerIndex,
                                                 _totalSigners, _requiredSigners);
    } else {
        return make_shared<MockupSigShare>(_sigShare, _schainID, _blockID, _signerNodeID, _signerIndex,
                                           _totalSigners, _requiredSigners);
    }
}

void CryptoManager::signProposalECDSA(BlockProposal* _proposal) {
    CHECK_ARGUMENT(_proposal != nullptr);
    auto signature = signECDSA(_proposal->getHash());
    CHECK_STATE( signature != nullptr);
    _proposal->addSignature(signature);
}

bool CryptoManager::verifyProposalECDSA(BlockProposal* _proposal, ptr<string> _hashStr, ptr<string> _signature) {
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

ptr<ThresholdSignature> CryptoManager::verifyThresholdSig(ptr<SHAHash> _hash, ptr<string> _signature, block_id _blockId) {
    MONITOR(__CLASS_NAME__, __FUNCTION__)

    auto requiredSigners = sChain->getRequiredSigners();
    auto totalSigners = sChain->getTotalSigners();


    if (getSchain()->getNode()->isBlsEnabled()) {

        auto hash = make_shared<std::array<uint8_t, 32>>();

        memcpy(hash->data(), _hash->data(), 32);



        auto sig = make_shared<ConsensusBLSSignature>(_signature , _blockId, requiredSigners, totalSigners);

        if (!sChain->getNode()->getBlsPublicKey()->VerifySig(hash,
                sig->getBlsSig(), requiredSigners, totalSigners)) {
            BOOST_THROW_EXCEPTION(InvalidArgumentException("BLS Signature did not verify",
                    __CLASS_NAME__));
        }
        return  sig;
    } else {

        auto sig = make_shared<MockupSignature>(_signature, _blockId, requiredSigners, totalSigners);

        if (*sig->toString() != *_hash->toHex()) {
            BOOST_THROW_EXCEPTION(InvalidArgumentException("Mockup threshold signature did not verify",
                                                           __CLASS_NAME__));
        }
        return  sig;
    }
}






