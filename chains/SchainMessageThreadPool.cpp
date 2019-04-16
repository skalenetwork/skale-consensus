//
// Created by stan on 18.03.18.
//


#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"

#include "../thirdparty/json.hpp"
#include "../abstracttcpserver/ConnectionStatus.h"
#include "../pendingqueue/PendingTransactionsAgent.h"
#include "../blockproposal/pusher/BlockProposalClientAgent.h"
#include "../blockproposal/received/ReceivedBlockProposalsDatabase.h"
#include "../threads/WorkerThreadPool.h"
#include "Schain.h"
#include "SchainMessageThreadPool.h"


SchainMessageThreadPool::SchainMessageThreadPool(void *params_) : WorkerThreadPool(NUM_SCHAIN_THREADS, params_) {

}

void SchainMessageThreadPool::createThread(uint64_t /*_threadNumber*/){
    threadpool.push_back( make_shared < thread > ( Schain::messageThreadProcessingLoop, reinterpret_cast < Schain * > ( params ) ) );
}
