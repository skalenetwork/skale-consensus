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

    @file TestMessageGeneratorAgent.cpp
    @author Stan Kladko
    @date 2018
*/

#include "pendingqueue/TestMessageGeneratorAgent.h"
#include "PendingTransactionsAgent.h"
#include "SkaleCommon.h"
#include "Log.h"
#include "chains/Schain.h"
#include "chains/SchainTest.h"
#include "crypto/CryptoManager.h"
#include "datastructures/Transaction.h"
#include "exceptions/FatalError.h"
#include "node/ConsensusEngine.h"
#include "thirdparty/json.hpp"
#include "oracle/OracleClient.h"


TestMessageGeneratorAgent::TestMessageGeneratorAgent(Schain& _sChain_) : Agent(_sChain_, false) {
    CHECK_STATE(_sChain_.getNodeCount() > 0);
}




ConsensusExtFace::transactions_vector TestMessageGeneratorAgent::pendingTransactions( size_t _limit ) {

    // test oracle for the first block

    uint64_t  messageSize = 200;

    ConsensusExtFace::transactions_vector result;

    auto test = sChain->getBlockProposerTest();

    CHECK_STATE(!test.empty());

    if (test == SchainTest::NONE)
        return result;

    for (uint64_t i = 0; i < _limit; i++) {

        vector<uint8_t> transaction(messageSize);

        if (i == 3) {
            auto magicStart = getSchain()->getCryptoManager()->getTeMagicStart();
            transaction.insert(transaction.end(), magicStart.cbegin(), magicStart.cend());
        }

        uint64_t  dummy = counter;
        auto bytes = (uint8_t*) & dummy;

        for (uint64_t j = 0; j < messageSize/8; j++) {
            for (int k = 0; k < 7; k++) {
                transaction.at(2 * j + k ) = bytes[k];
            }

        }

        if (i == 3) {
            auto magicEnd = getSchain()->getCryptoManager()->getTeMagicEnd();
            transaction.insert(transaction.end(), magicEnd.cbegin(), magicEnd.cend());
        }

        result.push_back(transaction);

        counter++;

    }

    static uint64_t iterations = 0;
    // send oracle test once from schain index 1
    /*
    if (iterations == 40) {
        LOG(info, "Sending Oracle test");
        getSchain()->getOracleClient()->sendTestRequestGet();
        sleep(1);
        getSchain()->getOracleClient()->sendTestRequestGet();
        LOG(info, "Sent Oracle test");
    }
     */
    iterations++;



    return result;

};


