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

    @file ReceivedDASigSharesDatabase.cpp
    @author Stan Kladko
    @date 2019
*/

#include "../../SkaleCommon.h"
#include "../../Agent.h"
#include "../../Log.h"
#include "../../exceptions/FatalError.h"
#include "../../thirdparty/json.hpp"

#include "../../crypto/ConsensusBLSSigShare.h"
#include "../../crypto/ConsensusBLSSignature.h"

#include "../../abstracttcpserver/ConnectionStatus.h"
#include "../../chains/Schain.h"
#include "../../node/Node.h"
#include "../../pendingqueue/PendingTransactionsAgent.h"
#include "../../crypto/ConsensusBLSSigShare.h"
#include "../../crypto/ConsensusSigShareSet.h"
#include "../../crypto/CryptoManager.h"
#include "../../crypto/SHAHash.h"
#include "leveldb/db.h"

#include "../../db/SigDB.h"
#include "ReceivedDASigSharesDatabase.h"
#include "BLSSigShare.h"
#include "BLSSignature.h"
#include "BLSSigShareSet.h"
#include "../../crypto/ThresholdSigShare.h"


using namespace std;


ReceivedDASigSharesDatabase::ReceivedDASigSharesDatabase(Schain &_sChain) {
    this->sChain = &_sChain;
};

ptr<ThresholdSignature> ReceivedDASigSharesDatabase::addAndMergeSigShare(ptr<ThresholdSigShare> _sigShare) {


    ASSERT(_sigShare);

    lock_guard<recursive_mutex> lock(sigShareDatabaseMutex);

    if (this->sigShareSets.count(_sigShare->getBlockId()) == 0) {

        auto set = sChain->getCryptoManager()->createSigShareSet(_sigShare->getBlockId(),
                                                                 sChain->getTotalSignersCount(),
                                                                 sChain->getRequiredSignersCount());

        sigShareSets[_sigShare->getBlockId()] = set;

        ASSERT(!set->isEnough());

        LOG(err, "Create set");
    }

    auto set = sigShareSets.at(_sigShare->getBlockId());

    if (set->isEnough()) { // already merged
        LOG(err, "IS ENOUGH");
        return nullptr;
    }

    LOG(err, "Adding sigshare");
    set->addSigShare(_sigShare);

    if (set->isEnough()) {
        LOG(err, "Merged signature");
        return set->mergeSignature();
    }

    return nullptr;
}




