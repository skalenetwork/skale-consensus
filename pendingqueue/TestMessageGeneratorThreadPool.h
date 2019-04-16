#pragma once

#include "../threads/WorkerThreadPool.h"

class TestMessageGeneratorThreadPool : public WorkerThreadPool {

public:

    TestMessageGeneratorThreadPool(num_threads numThreads, void *params_);

    virtual void createThread(uint64_t _threadNumber);

};