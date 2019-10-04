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

    @file ReceivedSigSharesDatabase.cpp
    @author Stan Kladko
    @date 2019
*/

#include "../SkaleCommon.h"
#include "../Agent.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../thirdparty/json.hpp"

#include "ConsensusBLSSigShare.h"
#include "ConsensusBLSSignature.h"

#include "../abstracttcpserver/ConnectionStatus.h"
#include "../chains/Schain.h"
#include "../node/Node.h"
#include "../pendingqueue/PendingTransactionsAgent.h"
#include "ConsensusBLSSigShare.h"
#include "ConsensusSigShareSet.h"
#include "SHAHash.h"
#include "leveldb/db.h"

#include "../db/SigDB.h"
#include "ReceivedSigSharesDatabase.h"
#include "BLSSigShare.h"
#include "BLSSignature.h"
#include "BLSSigShareSet.h"
#include "ThresholdSigShare.h"


using namespace std;



ReceivedSigSharesDatabase::ReceivedSigSharesDatabase(Schain &_sChain) : Agent(_sChain, true){
};



ptr<ThresholdSignature> ReceivedSigSharesDatabase::getBLSSignature(block_id _blockId) {
    lock_guard<recursive_mutex> lock(m);

    if (blockSignatures.find(_blockId) != blockSignatures.end()) {
        return blockSignatures.at(_blockId);
    } else {
        return nullptr;
    }
}

void ReceivedSigSharesDatabase::mergeAndSaveBLSSignature(block_id _blockId) {

    lock_guard<recursive_mutex> lock(m);

    if (getBLSSignature(_blockId)) {
        LOG(err, "Attempted to recreate block BLS signature");
        return;
    }

    auto sigSet = getSigShareSet(_blockId);
    ASSERT(sigSet->isEnough());
    auto signature = sigSet->mergeSignature();
    blockSignatures[_blockId] = signature;

    auto db = getNode()->getSignatureDB();

    db->addSignature(_blockId, signature);

    sigShareSets[_blockId] = nullptr;
}

bool ReceivedSigSharesDatabase::addSigShare(ptr<ThresholdSigShare> _sigShare) {


    ASSERT(_sigShare);

    LOG(trace, "addBlockProposal blockID_=" + to_string(_sigShare->getBlockId()) + " proposerIndex=" +
               to_string(_sigShare->getSignerIndex()));

    lock_guard<recursive_mutex> lock(m);

    if (this->sigShareSets.count(_sigShare->getBlockId()) == 0) {
        sigShareSets[_sigShare->getBlockId()] = make_shared<ConsensusSigShareSet>(_sigShare->getBlockId(),
                sChain->getTotalSignersCount(), sChain->getRequiredSignersCount());
    }

    sigShareSets.at(_sigShare->getBlockId())->addSigShare(_sigShare);

    return sigShareSets.at(_sigShare->getBlockId())->isEnoughMinusOne();

}






ptr<ConsensusSigShareSet> ReceivedSigSharesDatabase::getSigShareSet(block_id blockID) {


    lock_guard<recursive_mutex> lock(m);

    if (sigShareSets.count(blockID) == 0) {
        sigShareSets[blockID] = make_shared<ConsensusSigShareSet>(blockID,
            sChain->getTotalSignersCount(), sChain->getRequiredSignersCount());
    }

    return sigShareSets.at(blockID);
}

bool ReceivedSigSharesDatabase::isTwoThird(block_id _blockID) {


    lock_guard<recursive_mutex> lock(m);


    if (sigShareSets.count(_blockID) > 0) {
        return sigShareSets.at(_blockID)->isEnough();
    } else {
        return false;
    };
}
