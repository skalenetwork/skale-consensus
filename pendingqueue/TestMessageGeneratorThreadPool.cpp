#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"


#include "TestMessageGeneratorAgent.h"
#include "TestMessageGeneratorThreadPool.h"

TestMessageGeneratorThreadPool::TestMessageGeneratorThreadPool(num_threads numThreads, void *params_) : WorkerThreadPool(numThreads,
                                                                                                                       params_) {}

void TestMessageGeneratorThreadPool::createThread(uint64_t /*_threadNumber*/) {
    threadpool.push_back(make_shared<thread>(TestMessageGeneratorAgent::workerThreadMessagePushLoop, reinterpret_cast < TestMessageGeneratorAgent * > ( params )  ));
    LOG(debug, __CLASS_NAME__ + " worker bound");
}







