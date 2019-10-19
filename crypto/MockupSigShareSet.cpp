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

#include "../SkaleCommon.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "bls_include.h"
#include "../node/ConsensusEngine.h"
#include "SHAHash.h"
#include "MockupSignature.h"

#include "../chains/Schain.h"
#include "../pendingqueue/PendingTransactionsAgent.h"
#include "MockupSigShare.h"
#include "BLSSigShareSet.h"
#include "MockupSigShareSet.h"


using namespace std;


MockupSigShareSet::MockupSigShareSet(block_id _blockId, size_t _totalSigners, size_t _requiredSigners)
        : ThresholdSigShareSet(_blockId, _totalSigners, _requiredSigners){

    totalObjects++;
}

MockupSigShareSet::~MockupSigShareSet() {
    totalObjects--;
}


ptr<ThresholdSignature> MockupSigShareSet::mergeSignature() {

    auto sig = make_shared<string>(to_string(blockId));
    return make_shared<MockupSignature>(sig, blockId,
                                        totalSigners, requiredSigners);
}

bool MockupSigShareSet::isEnough() {
    return (sigShares.size() >= requiredSigners);
}


bool MockupSigShareSet::isEnoughMinusOne() {
    auto sigsCount = sigShares.size();
    return sigsCount >= requiredSigners - 1;
}


bool MockupSigShareSet::addSigShare(shared_ptr<ThresholdSigShare> _sigShare) {
    if (was_merged) {
        BOOST_THROW_EXCEPTION(std::runtime_error("Invalid state"));
    }

    if (!_sigShare) {
        BOOST_THROW_EXCEPTION(std::runtime_error("Null _sigShare"));
    }

    if (sigShares.count((uint64_t )_sigShare->getSignerIndex()) > 0) {
         return false;
    }

    ptr<MockupSigShare> mss = dynamic_pointer_cast<MockupSigShare>(_sigShare);

    CHECK_ARGUMENT(mss != nullptr);

    sigShares[(uint64_t )_sigShare->getSignerIndex()] = mss;

    return true;
}

