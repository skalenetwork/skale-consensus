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

    @file CryptoSigner.h
    @author Stan Kladko
    @date 2019

*/
#include "../SkaleCommon.h"
#include "../Log.h"
#include "../thirdparty/json.hpp"
#include "Schain.h"
#include "../crypto/SHAHash.h"
#include "../crypto/ConsensusBLSSigShare.h"
#include "../node/Node.h"
#include "../monitoring/LivelinessMonitor.h"
#include "../libBLS/bls/BLSPrivateKeyShare.h"

#include "CryptoSigner.h"


CryptoSigner::CryptoSigner(Schain& _sChain) : sChain(&_sChain) {
    CHECK_ARGUMENT(sChain != nullptr);
}

Schain *CryptoSigner::getSchain() const {
    return sChain;
}

ptr<ConsensusBLSSigShare> CryptoSigner::sign(ptr<SHAHash> _hash, block_id _blockId) {

    MONITOR(__CLASS_NAME__, __FUNCTION__)

    auto hash = make_shared<std::array<uint8_t,32>>();

    memcpy(hash->data(), _hash->data(), 32);

    auto blsShare = sChain->getNode()->getBlsPrivateKey()->sign(hash, (uint64_t) sChain->getSchainIndex());

    return make_shared<ConsensusBLSSigShare>(blsShare, sChain->getSchainID(), _blockId, sChain->getNode()->getNodeID());

}

