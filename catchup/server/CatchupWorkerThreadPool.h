#pragma  once






#include "../../threads/WorkerThreadPool.h"


class CatchupServerAgent;


class CatchupWorkerThreadPool : public WorkerThreadPool {

public:

    CatchupWorkerThreadPool(num_threads numThreads, void *params_);

    virtual void createThread(uint64_t threadNumber);
};


