#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"


#include "WorkerThreadPool.h"


vector<ptr<thread>> WorkerThreadPool::allThreads;

void WorkerThreadPool::startService() {

    for (uint64_t i = 0; i < (uint64_t )numThreads; i++) {
        createThread(i);
        allThreads.push_back(threadpool[i]);
    }

}


WorkerThreadPool::WorkerThreadPool(num_threads _numThreads, void* _param) {
   assert(_numThreads > 0);
    LOG(info, "Started threads count:" + to_string(_numThreads));
   this->params = _param;
   this->numThreads = _numThreads;
}

const vector<shared_ptr<thread>> &WorkerThreadPool::getAllThreads() {
    return allThreads;
}

void WorkerThreadPool::addThread(ptr<thread> _t) {
    allThreads.push_back(_t);
}
