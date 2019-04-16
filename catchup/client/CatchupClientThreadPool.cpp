#include "../../SkaleConfig.h"
#include "../../Log.h"
#include "../../Agent.h"
#include "../../exceptions/FatalError.h"


#include "../../abstracttcpserver/ConnectionStatus.h"
#include "../../network/Connection.h"

#include "../../thirdparty/json.hpp"

#include "CatchupClientAgent.h"
#include "CatchupClientThreadPool.h"

CatchupClientThreadPool::CatchupClientThreadPool(num_threads numThreads, void *params_) : WorkerThreadPool(numThreads,
                                                                                                            params_) {

}


void CatchupClientThreadPool::createThread(uint64_t /*number*/) {

    auto p = (CatchupClientAgent*)params;



    this->threadpool.push_back(make_shared<thread>(CatchupClientAgent::workerThreadItemSendLoop, p));

}


