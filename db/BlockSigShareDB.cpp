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


#include "../SkaleCommon.h"
#include "../Log.h"
#include "../thirdparty/json.hpp"
#include "../crypto/SHAHash.h"
#include "../crypto/ThresholdSignature.h"
#include "../chains/Schain.h"
#include "../node/Node.h"
#include "../node/NodeInfo.h"
#include "../crypto/ThresholdSigShareSet.h"
#include "../exceptions/InvalidStateException.h"
#include "../datastructures/CommittedBlock.h"
#include "../crypto/ThresholdSigShare.h"
#include "../crypto/CryptoManager.h"
#include "../exceptions/ExitRequestedException.h"

#include "BlockSigShareDB.h"


BlockSigShareDB::BlockSigShareDB(string &_dirName, string &_prefix, node_id _nodeId, uint64_t _maxDBSize,
                                 Schain *_sChain)
        : LevelDB(_dirName, _prefix, _nodeId, _maxDBSize), sChain(_sChain) {
    CHECK_ARGUMENT(sChain != nullptr);
}


ptr<ThresholdSignature>
BlockSigShareDB::checkAndSaveShare(ptr<ThresholdSigShare> _sigShare, ptr<CryptoManager> _cryptoManager) {
    try {
        CHECK_ARGUMENT(_sigShare != nullptr);
        CHECK_ARGUMENT(_cryptoManager != nullptr);
        auto sigShareString = _sigShare->toString();
        auto enoughSet = writeStringToBlockSet("", *sigShareString, _sigShare->getBlockId(),
                                               _sigShare->getSignerIndex(), sChain->getTotalSigners(),
                                               sChain->getRequiredSigners());
        if (enoughSet == nullptr)
            return nullptr;

        auto s = _cryptoManager->createSigShareSet(_sigShare->getBlockId(), sChain->getTotalSigners(),
                                                   sChain->getRequiredSigners());


        for (auto &&item : *enoughSet) {
            auto nodeInfo = sChain->getNode()->getNodeInfoByIndex(item.first);
            CHECK_STATE(nodeInfo != nullptr);
            auto sigShare = _cryptoManager->createSigShare(item.second, sChain->getSchainID(),
                                                           _sigShare->getBlockId(), nodeInfo->getNodeID(), item.first,
                                                           sChain->getTotalSigners(),
                                                           sChain->getRequiredSigners());
            s->addSigShare(sigShare);
        }


        CHECK_STATE(s->isEnough());
        auto signature = s->mergeSignature();
        CHECK_STATE(signature != nullptr);
        return signature;
    } catch (ExitRequestedException &) { throw; }
    catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


const string BlockSigShareDB::getFormatVersion() {
    return "1.0";
}






