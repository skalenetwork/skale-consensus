/*
    Copyright (C) 2018-2019 SKALE Labs

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

    @file TestMessageGeneratorAgent.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../thirdparty/json.hpp"
#include "../node/Node.h"
#include "../chains/Schain.h"
#include "../chains/SchainTest.h"
#include "../datastructures/PendingTransaction.h"
#include "../chains/Schain.h"
#include "../pendingqueue/TestMessageGeneratorAgent.h"
#include "../datastructures/Transaction.h"
#include "../node/ConsensusEngine.h"
#include "PendingTransactionsAgent.h"


TestMessageGeneratorAgent::TestMessageGeneratorAgent(Schain& _sChain_) : Agent(_sChain_, false) {

    ASSERT(_sChain_.getNodeCount() > 0);
    LOG(info, "created");
}



ConsensusExtFace::transactions_vector TestMessageGeneratorAgent::pendingTransactions( size_t _limit ) {

    uint64_t  messageSize = 200;

    ConsensusExtFace::transactions_vector result;


    if (*sChain->getBlockProposerTest() == SchainTest::NONE)
        return result;

    for (uint64_t i = 0; i < _limit; i++) {

        vector<uint8_t> transaction(messageSize);

        uint64_t  dummy = counter;

        for (uint64_t j = 0; j < messageSize/8; j++) {
            auto bytes = (uint8_t*) & dummy;
            for (int k = 0; k < 7; k++) {
                transaction.data()[2 * j + k ] = bytes[k];
            }

        }

        result.push_back(transaction);

        counter++;

    }

    return result;

};


