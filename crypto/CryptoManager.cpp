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
#include "SHAHash.h"
#include "ConsensusBLSSigShare.h"
#include "MockupSigShare.h"
#include "ConsensusSigShareSet.h"
#include "MockupSigShareSet.h"
#include "../node/Node.h"
#include "../monitoring/LivelinessMonitor.h"
#include "bls/BLSPrivateKeyShare.h"


#include "CryptoManager.h"


void CryptoManager::init() {

    BIO *bio;

    bio = BIO_new_mem_buf((void *) insecureTestECDSAKey, strlen(insecureTestECDSAKey));
    ecdsaKey = PEM_read_bio_ECPrivateKey(
            bio,
            NULL,
            NULL,
            NULL
    );

    if (ecdsaKey == nullptr) {
        throw FatalError("Could not init openssl key", __CLASS_NAME__);
    }

}


CryptoManager::CryptoManager() : sChain(nullptr) {
    init();



}


CryptoManager::CryptoManager(Schain &_sChain) : sChain(&_sChain) {
    CHECK_ARGUMENT(sChain != nullptr);
    init();


}


ptr<string> CryptoManager::sign(ptr<SHAHash> _hash) {

    auto sig = ECDSA_do_sign(_hash->getHash()->data(), SHA_HASH_LEN, ecdsaKey);

    CHECK_STATE(sig != nullptr);

    auto len = ECDSA_size(ecdsaKey);

    auto signature = (unsigned char *) malloc(len);

    auto ret = i2d_ECDSA_SIG(sig, &signature);

    CHECK_STATE(ret != 0);

    unsigned char encoded[2 * len];

    EVP_EncodeBlock(encoded, signature, len);

    return make_shared<string>((const char*)encoded);


}

Schain *CryptoManager::getSchain() const {
    return sChain;
}

ptr<ThresholdSigShare> CryptoManager::sign(ptr<SHAHash> _hash, block_id _blockId) {


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
                                           sChain->getTotalSignersCount(),
                                           sChain->getRequiredSignersCount());
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


const char *CryptoManager::insecureTestECDSAKey =
        "-----BEGIN EC PRIVATE KEY-----\n"
        "MHQCAQEEINbmHz6w9lvoNvgPPRwkVSJVAD0zS3Rhd2YMQl6fcLpFoAcGBSuBBAAK"
        "oUQDQgAEmtFhQ0RnjT1zQYhYUcKAi5j1E6wAu5dAo9pileYW0fgDX2533s1FUSiz"
        "Mg90hwa2Z50fcIxS9JY8SFuf+tllyQ==\n"
        "-----END EC PRIVATE KEY-----";






