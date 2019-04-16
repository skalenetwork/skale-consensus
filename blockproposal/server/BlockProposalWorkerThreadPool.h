//
// Created by stan on 18.12.18.
//

#pragma  once




#include "../../threads/WorkerThreadPool.h"


class BlockProposalServerAgent;


class BlockProposalWorkerThreadPool : public WorkerThreadPool {

public:

    BlockProposalWorkerThreadPool(num_threads numThreads, void *params_);

    virtual void createThread(uint64_t threadNumber);
};


