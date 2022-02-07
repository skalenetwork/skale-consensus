/*
    Copyright (C) 2018-2019 SKALE Labs

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

    @file Set.cpp
    @author Stan Kladko
    @date 2018-
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"

#include "node/ConsensusEngine.h"
#include "crypto/BLAKE3Hash.h"
#include "datastructures/BlockAesKeyDecryptionShare.h"
#include "datastructures/BooleanProposalVector.h"

#include "chains/Schain.h"
#include "pendingqueue/PendingTransactionsAgent.h"
#include "crypto/CryptoManager.h"
#include "datastructures/DAProof.h"
#include "datastructures/BlockEncryptedAesKeys.h"
#include "datastructures/BlockDecryptedAesKeys.h"
#include "BlockAesKeyDecryptionSet.h"

using namespace std;

ptr<BlockDecryptedAesKeys> BlockAesKeyDecryptionSet::add(const ptr<BlockAesKeyDecryptionShares> &_decryption,
                                                         Schain *_sChain) {

    CHECK_ARGUMENT(_decryption);
    CHECK_STATE(_sChain);

    auto index = (uint64_t) _decryption->getSchainIndex();

    CHECK_STATE(index > 0 && index <= nodeCount)

    LOCK(m)

    if (decryptions.count(index) > 0) {
        LOG(info,
            "Got block decryption with the same index" + to_string(index));
        return nullptr;
    }

    decryptions.emplace(index, _decryption);

    CHECK_STATE(encryptedKeys);

    if (isEnough()) {
        return mergeDecryptedKeyShares(_sChain);
    }
}

ptr<BlockDecryptedAesKeys>  BlockAesKeyDecryptionSet::mergeDecryptedKeyShares(const Schain *_sChain) {

    auto decryptedKeysMap = make_shared<map<uint64_t, ptr<vector<uint8_t>>>>();

    for (auto &&item: *encryptedKeys->getEncryptedKeys()) {

        auto transactionIndex = item.first;


        mergeDecryptedAesKeyForTransaction(_sChain, decryptedKeysMap, transactionIndex);
    }

    return make_shared<BlockDecryptedAesKeys>(decryptedKeysMap);
}

void BlockAesKeyDecryptionSet::mergeDecryptedAesKeyForTransaction(const Schain *_sChain,
                                                                  shared_ptr<map<uint64_t, ptr<vector<uint8_t>>>> &decryptedKeysMap,
                                                                  uint64_t transactionIndex) {

    auto sharesMap = make_shared<map<uint64_t, string>>();

    for (auto &&decryption: decryptions) {
        if (decryption.second->getData()->count(transactionIndex) > 0) {
            CHECK_STATE(sharesMap->emplace(decryption.first,
                                           decryption.second->getData()->at(transactionIndex)).second);
        }
    }

    if (sharesMap->size() == requiredDecryptionCount) {
        auto aesKey = _sChain->getCryptoManager()->teMergeDecryptedSharesIntoAESKey(
                sharesMap);
        decryptedKeysMap->emplace(transactionIndex, aesKey);
    }
                                                                  }

BlockAesKeyDecryptionSet::BlockAesKeyDecryptionSet(Schain *_sChain, block_id _blockId,
                                                   ptr<BlockEncryptedAesKeys> _encryptedKeys)
        : blockId(_blockId) {
    CHECK_ARGUMENT(_sChain);
    CHECK_ARGUMENT(_blockId > 0);

    encryptedKeys = _encryptedKeys;
    nodeCount = _sChain->getNodeCount();
    requiredDecryptionCount = _sChain->getRequiredSigners();
    totalObjects++;
}

BlockAesKeyDecryptionSet::~BlockAesKeyDecryptionSet() {
    totalObjects--;
}

node_count BlockAesKeyDecryptionSet::getCount() {
    LOCK(m)
    return (node_count) decryptions.size();
}


atomic<int64_t>  BlockAesKeyDecryptionSet::totalObjects(0);

bool BlockAesKeyDecryptionSet::isEnough() {
    LOCK(m)
    return decryptions.size() == this->requiredDecryptionCount;
}
