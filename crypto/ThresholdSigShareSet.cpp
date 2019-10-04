//
// Created by kladko on 04.10.19.
//

#include "../SkaleCommon.h"
#include "../Log.h"
#include "bls_include.h"
#include "../pendingqueue/PendingTransactionsAgent.h"
#include "../chains/Schain.h"
#include "ConsensusBLSSignature.h"
#include "../node/ConsensusEngine.h"
#include "../exceptions/FatalError.h"
#include "SHAHash.h"
#include "../libBLS/bls/BLSSigShare.h"
#include "../libBLS/bls/BLSSigShareSet.h"
#include "ConsensusSigShareSet.h"
#include "BLSSigShareSet.h"
#include "ConsensusBLSSigShare.h"


#include "ThresholdSigShareSet.h"

atomic< uint64_t > ThresholdSigShareSet::totalObjects( 0 );



uint64_t ThresholdSigShareSet::getTotalObjects() {
    return totalObjects;
}

ThresholdSigShareSet::ThresholdSigShareSet(const block_id &blockId, uint64_t requiredSigners, uint64_t totalSigners)
        : blockId(blockId), requiredSigners(requiredSigners), totalSigners(totalSigners) {}
