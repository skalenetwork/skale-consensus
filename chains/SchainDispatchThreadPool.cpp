#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"

#include "../node/ConsensusEngine.h"

#include "../threads/WorkerThreadPool.h"
#include "Schain.h"
#include "SchainDispatchThreadPool.h"


SchainDispatchThreadPool::SchainDispatchThreadPool(void *params_) : WorkerThreadPool(NUM_DISPATCH_THREADS, params_) {

}

void SchainDispatchThreadPool::createThread(uint64_t /*_threadNumber*/){
    //threadpool.push_back( make_shared<thread>(Schain::workerThreadDispatchProcessingLoop, reinterpret_cast < Schain * > (params) ) );
}
