/*
    Copyright (C) 2019 SKALE Labs

    This file is part of skale-Mockup.

    skale-Mockup is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-Mockup is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with skale-Mockup.  If not, see <https://www.gnu.org/licenses/>.

    @file MockupSigShareSet.cpp
    @author Stan Kladko
    @date 2019
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"
#include "bls_include.h"
#include "node/ConsensusEngine.h"
#include "SHAHash.h"
#include "MockupSignature.h"

#include "chains/Schain.h"
#include "pendingqueue/PendingTransactionsAgent.h"
#include "MockupSigShare.h"
#include "BLSSigShareSet.h"
#include "MockupSigShareSet.h"


using namespace std;


MockupSigShareSet::MockupSigShareSet(block_id _blockId, size_t _totalSigners, size_t _requiredSigners)
        : ThresholdSigShareSet(_blockId, _totalSigners, _requiredSigners){

    ASSERT(_requiredSigners > 0);
    ASSERT(_requiredSigners <= totalSigners);

    totalObjects++;
}

MockupSigShareSet::~MockupSigShareSet() {
    totalObjects--;
}


ptr<ThresholdSignature> MockupSigShareSet::mergeSignature() {

    LOCK(m)

    ptr<string> h = nullptr;

    for (auto&& item : sigShares) {
        if (h == nullptr) {
            h = item.second->toString();
        } else {
            CHECK_STATE(*h == *item.second->toString());
        }
    }
    ASSERT((h != nullptr));

    return make_shared<MockupSignature>(h, blockId,
                                        totalSigners, requiredSigners);
}

bool MockupSigShareSet::isEnough() {

    LOCK(m)

    return (sigShares.size() >= requiredSigners);
}





bool MockupSigShareSet::addSigShare(shared_ptr<ThresholdSigShare> _sigShare) {

    CHECK_ARGUMENT(_sigShare != nullptr);


    LOCK(m)


    if (isEnough())
       return false;

    if (sigShares.count((uint64_t )_sigShare->getSignerIndex()) > 0) {
         return false;
    }

    ptr<MockupSigShare> mss = dynamic_pointer_cast<MockupSigShare>(_sigShare);

    CHECK_STATE(mss != nullptr);

    sigShares[(uint64_t )_sigShare->getSignerIndex()] = mss;

    return true;
}

