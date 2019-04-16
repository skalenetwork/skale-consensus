#pragma once




#include <cstdint>

#include "../../threads/WorkerThreadPool.h"

class CatchupClientThreadPool : public WorkerThreadPool {

public:

    CatchupClientThreadPool(num_threads numThreads, void *params_);

    void createThread(uint64_t number) override;

};
