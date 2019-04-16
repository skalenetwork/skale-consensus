#pragma once

#include "../../threads/WorkerThreadPool.h"

class BlockProposalPusherThreadPool : public WorkerThreadPool {

public:

    BlockProposalPusherThreadPool(num_threads numThreads, void *params_);

    void createThread(uint64_t number);

};
