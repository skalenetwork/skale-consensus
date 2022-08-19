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

    @file DASigShareDB.cpp
    @author Stan Kladko
    @date 2019
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"
#include "thirdparty/json.hpp"


#include "crypto/ConsensusBLSSignature.h"
#include "chains/Schain.h"
#include "node/Node.h"
#include "pendingqueue/PendingTransactionsAgent.h"
#include "crypto/ConsensusSigShareSet.h"
#include "crypto/CryptoManager.h"
#include "crypto/BLAKE3Hash.h"
#include "datastructures/BlockProposal.h"


#include "leveldb/db.h"

#include "crypto/ThresholdSigShare.h"
#include "datastructures/DAProof.h"

#include "SigDB.h"
#include "DASigShareDB.h"


using namespace std;


DASigShareDB::DASigShareDB(Schain *_sChain, string &_dirName, string &_prefix, node_id _nodeId, uint64_t _maxDBSize) :
        CacheLevelDB(_sChain, _dirName, _prefix, _nodeId, _maxDBSize) {
};

const string& DASigShareDB::getFormatVersion() {
    static const string version = "1.0";
    return version;
}


// return not-null if _sigShare completes sig, null otherwise (both if not enough and too much)
ptr<DAProof> DASigShareDB::addAndMergeSigShareAndVerifySig(const ptr<ThresholdSigShare>& _sigShare,
                                                           const ptr<BlockProposal>& _proposal) {

    CHECK_ARGUMENT(_sigShare);
    CHECK_ARGUMENT(_proposal);

    LOCK(sigShareMutex)

    LOG(trace, "Adding sigshare");

    auto result = this->writeStringToSet(_sigShare->toString(),
            _sigShare->getBlockId(), _sigShare->getSignerIndex());

    if (result != nullptr) {

        auto set = sChain->getCryptoManager()->createDAProofSigShareSet(_sigShare->getBlockId());

        for (auto && entry : *result) {
            auto share = sChain->getCryptoManager()->createDAProofSigShare(
                entry.second, sChain->getSchainID(), _proposal->getBlockID(), entry.first, false );

            set->addSigShare(share);
        }


        LOG(trace, "Merged signature");
        auto sig = set->mergeSignature();
        CHECK_STATE(sig);



        auto h = _proposal->getHash();

        sChain->getCryptoManager()->verifyDAProofThresholdSig(
                h, sig->toString(), _sigShare->getBlockId());
        auto proof = make_shared<DAProof>(_proposal, sig);
        return proof;
    }

    return nullptr;
}




