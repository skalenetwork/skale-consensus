#include "../../SkaleConfig.h"
#include "../../Log.h"
#include "../../exceptions/FatalError.h"

#include "../../thirdparty/json.hpp"

#include "../../abstracttcpserver/ConnectionStatus.h"
#include "BlockProposalClientAgent.h"
#include "BlockProposalPusherThreadPool.h"

BlockProposalPusherThreadPool::BlockProposalPusherThreadPool(num_threads numThreads, void *params_) : WorkerThreadPool(numThreads,
                                                                                                            params_) {
    ASSERT(((BlockProposalClientAgent*) params)->queueMutex.size() > 0);
}


void BlockProposalPusherThreadPool::createThread(uint64_t /*number*/) {

    auto p = (BlockProposalClientAgent*)params;

    ASSERT(p->queueMutex.size() > 0);

    this->threadpool.push_back(make_shared<thread>(AbstractClientAgent::workerThreadItemSendLoop, p));

}


