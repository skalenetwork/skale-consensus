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
#include "../../datastructures/BlockProposal.h"


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

ptr<ThresholdSignature> ReceivedDASigSharesDatabase::addAndMergeSigShare(ptr<ThresholdSigShare> _sigShare,
        ptr<BlockProposal> _proposal) {


    ASSERT(_sigShare);

    LOCK(m)

    if (this->sigShareSets.count(_sigShare->getBlockId()) == 0) {

        auto set = sChain->getCryptoManager()->createSigShareSet(_sigShare->getBlockId(),
                                                                 sChain->getTotalSignersCount(),
                                                                 sChain->getRequiredSignersCount());

        sigShareSets[_sigShare->getBlockId()] = set;

        ASSERT(!set->isEnough());

    }

    auto set = sigShareSets.at(_sigShare->getBlockId());

    if (set->isEnough()) { // already merged
        return nullptr;
    }

    LOG(info, "Adding sigshare");
    set->addSigShare(_sigShare);

    if (set->isEnough()) {
        LOG(info, "Merged signature");
        auto sig = set->mergeSignature();

        sChain->getCryptoManager()->verifyThreshold(
                _proposal->getHash(), sig->toString(), _sigShare->getBlockId());
        return sig;
    }

    return nullptr;
}




