/*
    Copyright (C) 2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skale-consensus.  If not, see <http://www.gnu.org/licenses/>.

    @file ReceivedSigSharesDatabase.cpp
    @author Stan Kladko
    @date 2019
*/

#include "../SkaleConfig.h"
#include "../Agent.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../thirdparty/json.hpp"

#include "BLSSigShare.h"
#include "BLSSignature.h"

#include "../abstracttcpserver/ConnectionStatus.h"
#include "leveldb/db.h"
#include "../node/Node.h"
#include "../chains/Schain.h"
#include "SHAHash.h"
#include "BLSSigShare.h"
#include "../pendingqueue/PendingTransactionsAgent.h"
#include "../datastructures/SigShareSet.h"

#include "ReceivedSigSharesDatabase.h"


using namespace std;



ReceivedSigSharesDatabase::ReceivedSigSharesDatabase(Schain &_sChain) : Agent(_sChain, true){
};



ptr<BLSSignature> ReceivedSigSharesDatabase::getBLSSignature(block_id _blockId) {
    lock_guard<recursive_mutex> lock(sigShareDatabaseMutex);

    if (blockSignatures.find(_blockId) != blockSignatures.end()) {
        return blockSignatures[_blockId];
    } else {
        return nullptr;
    }
}

void ReceivedSigSharesDatabase::mergeAndSaveBLSSignature(block_id _blockId) {

    lock_guard<recursive_mutex> lock(sigShareDatabaseMutex);

    if (getBLSSignature(_blockId)) {
        LOG(err, "Attempted to recreate block BLS signature");
        return;
    }

    auto sigSet = getSigShareSet(_blockId);
    assert(sigSet->isTwoThird());
    auto signature = sigSet->mergeSignature();
    blockSignatures[_blockId] = signature;

    auto db = getNode()->getSignaturesDB();
    auto key = to_string(_blockId);
    if (db->readString(key) == nullptr)
    getNode()->getSignaturesDB()->writeString(key, *signature->toString());

    sigShareSets[_blockId] = nullptr;
}

bool ReceivedSigSharesDatabase::addSigShare(ptr<BLSSigShare> _sigShare) {


    ASSERT(_sigShare);

    LOG(trace, "addBlockProposal blockID_=" + to_string(_sigShare->getBlockId()) + " proposerIndex=" +
               to_string(_sigShare->getSignerIndex()));

    lock_guard<recursive_mutex> lock(sigShareDatabaseMutex);

    if (this->sigShareSets.count(_sigShare->getBlockId()) == 0) {
        sigShareSets[_sigShare->getBlockId()] = make_shared<SigShareSet>(this->sChain, _sigShare->getBlockId());
    }

    sigShareSets[_sigShare->getBlockId()]->addSigShare(_sigShare);


    return (sigShareSets[_sigShare->getBlockId()]->isTwoThirdMinusOne());
}






ptr<SigShareSet> ReceivedSigSharesDatabase::getSigShareSet(block_id blockID) {


    lock_guard<recursive_mutex> lock(sigShareDatabaseMutex);

    if (sigShareSets.count(blockID) == 0) {
        sigShareSets[blockID] = make_shared<SigShareSet>(this->sChain, blockID);
    }

    return sigShareSets[blockID];
}



bool ReceivedSigSharesDatabase::isTwoThird(block_id _blockID) {


    lock_guard<recursive_mutex> lock(sigShareDatabaseMutex);


    if (sigShareSets.count(_blockID) > 0) {
        return sigShareSets[_blockID]->isTwoThird();
    } else {
        return false;
    };
}
