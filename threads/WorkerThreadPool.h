#pragma once



class Schain;


class WorkerThreadPool {
protected:
    static vector< ptr< thread > > allThreads;


public:


    static void addThread(ptr<thread> _t);


    static const vector< shared_ptr< thread > >& getAllThreads();

protected:
    vector< ptr< thread > > threadpool;

    num_threads numThreads;

    void* params;

public:
    WorkerThreadPool( num_threads _numThreads, void* _param_ );
    virtual ~WorkerThreadPool() {}

    void startService();

    virtual void createThread( uint64_t threadNumber ) = 0;
};
