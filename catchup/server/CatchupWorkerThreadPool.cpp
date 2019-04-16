#include "../../SkaleConfig.h"
#include "../../Agent.h"
#include "../../Log.h"
#include "../../exceptions/FatalError.h"
#include "../../thirdparty/json.hpp"
#include "../../abstracttcpserver/AbstractServerAgent.h"
#include "../../threads/WorkerThreadPool.h"
#include "CatchupServerAgent.h"


CatchupWorkerThreadPool::CatchupWorkerThreadPool(num_threads numThreads, void *params_) : WorkerThreadPool(numThreads, params_) {

    ASSERT(numThreads > 0);

}


void CatchupWorkerThreadPool::createThread(uint64_t /*threadNumber*/) {

    this->threadpool.push_back(make_shared<thread>(AbstractServerAgent::workerThreadConnectionProcessingLoop,
                                                   (CatchupServerAgent*)params));
}
