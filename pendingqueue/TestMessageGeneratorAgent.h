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

    @file TestMessageGeneratorAgent.h
    @author Stan Kladko
    @date 2018
*/

#pragma  once


#include "Agent.h"
#include "node/ConsensusEngine.h"

class Schain;

class TestMessageGeneratorAgent : Agent {


    uint64_t counter = 0;


public:


    TestMessageGeneratorAgent(Schain& _sChain);

    ConsensusExtFace::transactions_vector pendingTransactions( size_t _limit);

};
