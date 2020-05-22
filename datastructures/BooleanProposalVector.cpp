/*
    Copyright (C) 2018-2019 SKALE Labs

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

    @file BooleanProposalVector.cpp
    @author Stan Kladko
    @date 2019
*/

#include "SkaleCommon.h"
#include "SkaleLog.h"
#include "exceptions/FatalError.h"

#include "BooleanProposalVector.h"


BooleanProposalVector::BooleanProposalVector(node_count _nodeCount, ptr<map<schain_index, ptr<string>>>
_receivedDAProofs) : nodeCount(_nodeCount) {
    CHECK_ARGUMENT(_receivedDAProofs != nullptr);

    proposals.push_back(false);

    for (uint64_t i = 1; i <= _nodeCount; i++) {
        proposals.push_back(_receivedDAProofs->count(schain_index(i)) > 0);
    }

    trueCount = proposals.size();
}

BooleanProposalVector::BooleanProposalVector(node_count _nodeCount, ptr<string> _vectorStr)
        : nodeCount(_nodeCount) {
    CHECK_ARGUMENT(_vectorStr != nullptr);
    CHECK_ARGUMENT(_vectorStr->size() == _nodeCount);
    proposals.push_back(false);

    for (uint64_t i = 1; i <= _nodeCount; i++) {
        auto value = _vectorStr->at(i - 1);
        if (value == '1') {
            proposals.push_back(true);
            trueCount++;
        } else if (value == '0') {
            proposals.push_back(false);
        } else {
            BOOST_THROW_EXCEPTION(InvalidArgumentException("Corrupt char in vector:" + to_string(value),
                                                           __CLASS_NAME__));

        }
    }
}


bool BooleanProposalVector::getProposalValue(schain_index _index) {
    ASSERT(proposals.size() == nodeCount + 1);
    ASSERT(_index <= (uint64_t) nodeCount);
    ASSERT(_index > 0);
    return proposals.at((uint64_t) _index);
}

uint64_t BooleanProposalVector::getTrueCount() const {
    return trueCount;
}

ptr<string> BooleanProposalVector::toString() {

    auto vectorStr = make_shared<string>();
    for (uint64_t i = 1; i <= nodeCount; i++) {
        auto value = (proposals.at(i) ? "1" : "0");
        vectorStr->append(value);
    }

    return vectorStr;
}
