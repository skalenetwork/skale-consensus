#pragma once

#include "../../threads/WorkerThreadPool.h"

class BlockFinalizeClientThreadPool : public WorkerThreadPool {

public:

    BlockFinalizeClientThreadPool(num_threads numThreads, void *params_);

    void createThread(uint64_t number);

};
