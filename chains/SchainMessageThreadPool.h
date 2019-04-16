#pragma  once





class Schain;
class WorkerThreadPool;


class SchainMessageThreadPool : public WorkerThreadPool {
public:

    SchainMessageThreadPool(void *params_);

    virtual void createThread(uint64_t _numThreads);
};

