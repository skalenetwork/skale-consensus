#pragma  once

#include "../Agent.h"

class TestMessageGeneratorThreadPool;
class Schain;

class TestMessageGeneratorAgent : Agent {


    uint64_t counter = 0;


public:

    static void workerThreadMessagePushLoop(TestMessageGeneratorAgent *_agent);

    ptr<TestMessageGeneratorThreadPool> testMessageGeneratorThreadPool = nullptr;

    TestMessageGeneratorAgent(Schain& _sChain);

    Schain *getSChain();

};
