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

    @file BlockSigShareDB.cpp
    @author Stan Kladko
    @date 2019
*/

#include "thirdparty/json.hpp"
#include "SkaleCommon.h"
#include "Log.h"
#include "chains/Schain.h"
#include "crypto/CryptoManager.h"
#include "crypto/BLAKE3Hash.h"
#include "crypto/ThresholdSigShare.h"
#include "crypto/ThresholdSigShareSet.h"
#include "crypto/ThresholdSignature.h"
#include "datastructures/CommittedBlock.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/InvalidStateException.h"
#include "node/Node.h"
#include "node/NodeInfo.h"


#include "LevelDBOptions.h"
#include "BlockSigShareDB.h"


BlockSigShareDB::BlockSigShareDB(Schain *_sChain, string &_dirName, string &_prefix, node_id _nodeId,
                                 uint64_t _maxDBSize)
        : CacheLevelDB(_sChain, _dirName, _prefix, _nodeId, _maxDBSize, LevelDBOptions::getBlockSigShareDBOptions() , false),
      sigShares(256){
}


ptr<ThresholdSignature>
BlockSigShareDB::checkAndSaveShareInMemory(const ptr<ThresholdSigShare>& _sigShare, const ptr<CryptoManager>& _cryptoManager,
                                           schain_index _proposer) {
    try {
        CHECK_ARGUMENT(_sigShare)
        CHECK_ARGUMENT(_cryptoManager)



        auto hash = BLAKE3Hash::getConsensusHash(
                (uint64_t) _proposer,
                (uint64_t) _sigShare->getBlockId(),
                (uint64_t) getSchain()->getSchainID());

        _cryptoManager->verifyThresholdSigShare(_sigShare, hash);


        auto sigShareString = _sigShare->toString();
        CHECK_STATE(!sigShareString.empty())

        LOCK(sigShareMutex)

        auto enoughSet = writeStringToSetInMemory(sigShareString, _sigShare->getBlockId(),
                                          _sigShare->getSignerIndex(), _proposer);
        if (enoughSet == nullptr)
            return nullptr;

        auto _sigShareSet = _cryptoManager->createSigShareSet(_sigShare->getBlockId());
        CHECK_STATE(_sigShareSet)

        for (auto &&item : *enoughSet) {

            auto nodeInfo = sChain->getNode()->getNodeInfoByIndex(item.first);
            CHECK_STATE(nodeInfo)
            CHECK_STATE(!item.second.empty())
            auto sigShare = _cryptoManager->createSigShare(item.second, sChain->getSchainID(),
                                                           _sigShare->getBlockId(), item.first, false);
            CHECK_STATE(sigShare)
            _sigShareSet->addSigShare(sigShare);
        }

        CHECK_STATE(_sigShareSet->isEnough())
        auto signature = _sigShareSet->mergeSignature();

        CHECK_STATE(signature)



        _cryptoManager->verifyThresholdSig(signature, hash);



        return signature;
    } catch (ExitRequestedException &) { throw; } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


ptr<ThresholdSignature>
BlockSigShareDB::checkAndSaveShare1(const ptr<ThresholdSigShare>& _sigShare, const ptr<CryptoManager>& _cryptoManager) {
    try {
        CHECK_ARGUMENT(_sigShare)
        CHECK_ARGUMENT(_cryptoManager)

        auto sigShareString = _sigShare->toString();
        CHECK_STATE(!sigShareString.empty())

        LOCK(sigShareMutex)

        auto enoughSet = writeStringToSet(sigShareString, _sigShare->getBlockId(),
                                          _sigShare->getSignerIndex());
        if (enoughSet == nullptr)
            return nullptr;

        auto _sigShareSet = _cryptoManager->createSigShareSet(_sigShare->getBlockId());
        CHECK_STATE(_sigShareSet)

        for (auto &&item : *enoughSet) {

            auto nodeInfo = sChain->getNode()->getNodeInfoByIndex(item.first);
            CHECK_STATE(nodeInfo)
            CHECK_STATE(!item.second.empty())
            auto sigShare = _cryptoManager->createSigShare(item.second, sChain->getSchainID(),
                                                           _sigShare->getBlockId(), item.first, false);
            CHECK_STATE(sigShare)
            _sigShareSet->addSigShare(sigShare);
        }

        CHECK_STATE(_sigShareSet->isEnough())
        auto signature = _sigShareSet->mergeSignature();
        CHECK_STATE(signature)
        return signature;
    } catch (ExitRequestedException &) { throw; } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


ptr<map<schain_index, string>>
BlockSigShareDB::writeStringToSetInMemory(const string &_value, block_id _blockId, schain_index _index,
                                          schain_index _proposerIndex) {


    CHECK_ARGUMENT(_index > 0 && _index <= totalSigners);

    LOCK(sigShareMutex);

    auto alreadySignedKey = createKey(_blockId);

    if (sigShares.exists(alreadySignedKey))
        return nullptr;

    auto entryKey = createKey(_blockId, _proposerIndex, _index);
    CHECK_STATE(entryKey != "");

    if (sigShares.exists(entryKey)) {
        if (!isDuplicateAddOK)
            LOG(trace, "Double db entry " + this->prefix + "\n" + to_string(_blockId) + ":" + to_string(_index));
        return nullptr;
    }

    uint64_t count = 0;

    auto counterKey = createKey(_blockId, _proposerIndex);

    if (sigShares.exists(counterKey)) {
        try {
            count = stoull(sigShares.get(counterKey), NULL, 10);
        } catch (...) {
            LOG(err, "Incorrect value in LevelDB:" + sigShares.get(counterKey));
            return 0;
        }
    } else {
        sigShares.put(counterKey, "0");
        count = 0;
    }

    count++;

    sigShares.put(counterKey, to_string(count));
    sigShares.put(entryKey,_value);

    if (count != requiredSigners) {
        return nullptr;
    }

    auto enoughSet = make_shared<map<schain_index, string>>();

    for (uint64_t i = 1; i <= totalSigners; i++) {
        auto key = createKey(_blockId,  _proposerIndex, schain_index(i));

        if (sigShares.exists(key)) {
            auto entry = sigShares.get( key );
            ( *enoughSet )[schain_index( i )] = entry;
        }
        if (enoughSet->size() == requiredSigners) {
            break;
        }
    }

    CHECK_STATE(enoughSet->size() == requiredSigners);

    sigShares.put(alreadySignedKey, "1");

    return enoughSet;
}



const string& BlockSigShareDB::getFormatVersion() {
    static const string version = "1.0";
    return version;
}






