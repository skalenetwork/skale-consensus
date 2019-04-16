#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../thirdparty/json.hpp"
#include "../node/Node.h"
#include "../chains/Schain.h"
#include "../chains/SchainTest.h"
#include "TestMessageGeneratorThreadPool.h"
#include "../datastructures/PendingTransaction.h"
#include "../chains/Schain.h"
#include "../pendingqueue/TestMessageGeneratorAgent.h"
#include "../datastructures/Transaction.h"
#include "PendingTransactionsAgent.h"


TestMessageGeneratorAgent::TestMessageGeneratorAgent(Schain& _sChain_) : Agent(_sChain_, false) {

    ASSERT(_sChain_.getNodeCount() > 0);


    //counter = _sChain_.getPendingTransactionsAgent()->getCommittedTransactionCounter();

    testMessageGeneratorThreadPool = make_shared<TestMessageGeneratorThreadPool>(num_threads(1), this);
    testMessageGeneratorThreadPool->startService();
    LOG(info, "created");
}

Schain *TestMessageGeneratorAgent::getSChain() {
    return sChain;
}


void TestMessageGeneratorAgent::workerThreadMessagePushLoop(TestMessageGeneratorAgent *_agent) {


    setThreadName(__CLASS_NAME__);

    _agent->waitOnGlobalStartBarrier();

    if (*_agent->sChain->getBlockProposerTest() == SchainTest::NONE)
        return;

    uint64_t  messageSize = 200;


    while (!_agent->getNode()->isExitRequested()) {

        auto transaction = make_shared<vector<uint8_t>>(messageSize);


        uint64_t  dummy = _agent->counter;

        for (uint64_t i = 0; i < messageSize/8; i++) {
            auto bytes = (uint8_t*) & dummy;
            for (int j = 0; j < 7; j++) {
                transaction->data()[2 * i + j ] = bytes[j];
            }

        }


        auto message = make_shared<PendingTransaction>(transaction);


//!!!        _agent->sChain->getPendingTransactionsAgent()->pushTransaction(message);

        _agent->counter++;



    };



};


