#include "../../SkaleConfig.h"
#include "../../Agent.h"
#include "../../Log.h"

#include "../../thirdparty/json.hpp"
#include "../../abstracttcpserver/AbstractServerAgent.h"
#include "../../exceptions/FatalError.h"

#include "../../threads/WorkerThreadPool.h"
#include "BlockProposalServerAgent.h"
#include "BlockProposalWorkerThreadPool.h"


BlockProposalWorkerThreadPool::BlockProposalWorkerThreadPool(num_threads numThreads, void *params_) : WorkerThreadPool(numThreads, params_) {

    ASSERT(numThreads > 0);

}


void BlockProposalWorkerThreadPool::createThread(uint64_t /*threadNumber*/) {

    this->threadpool.push_back(make_shared<thread>(AbstractServerAgent::workerThreadConnectionProcessingLoop,
                                                   (BlockProposalServerAgent*)params));
}
