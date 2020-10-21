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

    @file BooleanProposalVector.h
    @author Stan Kladko
    @date 2018
*/

#pragma  once

#include "DataStructure.h"



class BooleanProposalVector : public DataStructure {



private:

    uint64_t nodeCount = 0;
    uint64_t  trueCount = 0;
    vector<bool> proposals;

public:

    BooleanProposalVector(node_count _nodeCount, const ptr<map<schain_index, string>>& _receivedDAProofs);

    BooleanProposalVector(node_count _nodeCount, const string& _vectorStr);

    bool getProposalValue(schain_index _index);

    uint64_t getTrueCount() const;

    string toString();
};

