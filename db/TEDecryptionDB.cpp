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

    @file TEDecryptionsDB.cpp
    @author Stan Kladko
    @date 2019
*/


#ifdef BITE

#include "TEDecryptionDB.h"

#include "SkaleCommon.h"
#include "Log.h"
#include "thirdparty/json.hpp"
#include "chains/Schain.h"
#include "datastructures/BlockProposal.h"
#include "crypto/DecryptedAESKeyList.h"
#include "crypto/AESKeyDecryptionShareList.h"


#include "leveldb/db.h"
#include "crypto/ThresholdSigShare.h"
#include "crypto/AESKeyDecryptionShareList.h"
#include "LevelDBOptions.h"


#include <bite/BiteManager.h>
#include <bite/BiteAESDecryptionShareSerializer.h>
#include <crypto/AESKeyDecryptionShareSet.h>
#include "TEDecryptionDB.h"


using namespace std;


TEDecryptionDB::TEDecryptionDB(
    Schain *_sChain, string &_dirName, string &_prefix, node_id _nodeId, uint64_t _maxDBSize)
    : CacheLevelDB(_sChain, _dirName, _prefix, _nodeId, _maxDBSize,
                   LevelDBOptions::getTEDecryptionDBOptions(), false) {
}

const string &TEDecryptionDB::getFormatVersion() {
    static const string version = "1.0";
    return version;
}


bool TEDecryptionDB::haveDecryptions(const ptr<BlockProposal> &_proposal) {
    CHECK_ARGUMENT(_proposal)
    return keyExistsInSet(_proposal->getBlockID(), _proposal->getProposerIndex());
}


ptr<AESKeyDecryptionShareList> TEDecryptionDB::deserializeDecryptionShareFromString(string decryptions) {
    auto decryptionsVec = std::make_shared<std::vector<uint8_t> >(
        reinterpret_cast<const uint8_t *>(decryptions.data()),
        reinterpret_cast<const uint8_t *>(decryptions.data()) + decryptions.size()
    );
    return BiteAESDecryptionShareSerializer::deserialize(decryptionsVec,
                                                         getSchain()->getCryptoManager(), false);
}

ptr<AESKeyDecryptionShareList> TEDecryptionDB::getDecryptions(block_id _blockId, schain_index _decryptorIndex) {
    auto decryptions = readStringFromSet(_blockId, _decryptorIndex);
    return deserializeDecryptionShareFromString(decryptions);
}


ptr<DecryptedAESKeyList> TEDecryptionDB::addDecryptionShares(
    const ::std::shared_ptr<AESKeyDecryptionShareList> &_decryptionShareList) {
    CHECK_ARGUMENT(_decryptionShareList)

    LOG(trace, "Adding daProof");

    auto serializedList = BiteAESDecryptionShareSerializer::serialize(_decryptionShareList);
    CHECK_STATE(serializedList);


    auto decryptionShareListSet = this->writeByteArrayToSet(reinterpret_cast<char *>(serializedList->data()),
                                                            serializedList->size(),
                                                            _decryptionShareList->getBlockId(),
                                                            _decryptionShareList->getDecryptorIndex());

    if (!decryptionShareListSet) {
        return nullptr;
    }

    CHECK_STATE(decryptionShareListSet->size() == requiredSigners)

    map<schain_index, ptr<AESKeyDecryptionShareList> > decryptionShareLists;

    for (auto &&decryptionShareIterator: *decryptionShareListSet) {
        auto decryptionShare = deserializeDecryptionShareFromString(decryptionShareIterator.second);
        CHECK_STATE(decryptionShare);
        decryptionShareLists[decryptionShareIterator.first] = decryptionShare;
    }

    auto myDecryptionShareList = decryptionShareLists.at(sChain->getSchainIndex());
    CHECK_STATE(myDecryptionShareList);

    map<transaction_index, ptr<AESKeyDecryptionShareSet> > decryptionShareSets;


    for (auto &&decryptionShareIterator: myDecryptionShareList->getDecryptionShares()) {
        decryptionShareSets[decryptionShareIterator.first] = sChain->getBiteManager()->createAESDecryptionShareSet(
            _decryptionShareList->getBlockId(),
            decryptionShareIterator.first);
    }

    for (auto &&decryptionShareIterator: myDecryptionShareList->getDecryptionShares()) {
        decryptionShareSets[decryptionShareIterator.first] = sChain->getBiteManager()->createAESDecryptionShareSet(
            _decryptionShareList->getBlockId(),
            decryptionShareIterator.first);
    }

    for (auto&& it : decryptionShareLists) {
        auto decryptionSharesList = it.second;
        CHECK_STATE(decryptionSharesList);
        for (auto&& shareIterator : decryptionSharesList->getDecryptionShares()) {
            CHECK_STATE(decryptionShareSets.count(shareIterator.first > 0));
            auto decryptionSharesSet = decryptionShareSets.at(shareIterator.first);
            decryptionSharesSet->addDecryptionShare(shareIterator.second);
        }
    }


    auto aesKeys = make_shared<DecryptedAESKeyList>();

    for (auto&& it : decryptionShareSets) {
        CHECK_STATE(it.second->isEnough());
        auto key = it.second->mergeAESKey();
        CHECK_STATE(key);
        aesKeys->addKey(it.first, *key);
    }

    return aesKeys;

}


bool TEDecryptionDB::isEnoughDecryptions(block_id _blockID) {
    return isEnough(_blockID);
}



#endif
