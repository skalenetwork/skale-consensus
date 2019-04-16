#include "../../SkaleConfig.h"
#include "../../Agent.h"
#include "../../Log.h"
#include "../../exceptions/FatalError.h"
#include "../../thirdparty/json.hpp"

#include "../../crypto/BLSSigShare.h"
#include "../../crypto/BLSSignature.h"

#include "../../abstracttcpserver/ConnectionStatus.h"
#include "leveldb/db.h"
#include "../../node/Node.h"
#include "../../chains/Schain.h"
#include "../../crypto/SHAHash.h"
#include "../../crypto/BLSSigShare.h"
#include "../../pendingqueue/PendingTransactionsAgent.h"
#include "../../datastructures/SigShareSet.h"

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
