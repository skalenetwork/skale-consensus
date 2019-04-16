#pragma  once





class Schain;
class WorkerThreadPool;


class SchainDispatchThreadPool : public WorkerThreadPool {
public:

    SchainDispatchThreadPool(void *params_);

    virtual void createThread(uint64_t _numThreads);
};

