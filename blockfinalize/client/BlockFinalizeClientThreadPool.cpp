#include "../../SkaleConfig.h"
#include "../../Log.h"
#include "../../exceptions/FatalError.h"

#include "../../thirdparty/json.hpp"

#include "../../abstracttcpserver/ConnectionStatus.h"
#include "../../crypto/BLSSigShare.h"
#include "BlockFinalizeClientAgent.h"
#include "BlockFinalizeClientThreadPool.h"

BlockFinalizeClientThreadPool::BlockFinalizeClientThreadPool(num_threads numThreads, void *params_) : WorkerThreadPool(numThreads,
                                                                                                            params_) {
    ASSERT(((BlockFinalizeClientAgent*) params)->queueMutex.size() > 0);
}


void BlockFinalizeClientThreadPool::createThread(uint64_t /*number*/) {

    auto p = (BlockFinalizeClientAgent*)params;

    ASSERT(p->queueMutex.size() > 0);

    this->threadpool.push_back(make_shared<thread>(AbstractClientAgent::workerThreadItemSendLoop, p));

}


