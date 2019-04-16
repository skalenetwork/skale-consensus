/*
    Copyright (C) 2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skale-consensus.  If not, see <http://www.gnu.org/licenses/>.

    @file BlockFinalizeClientThreadPool.cpp
    @author Stan Kladko
    @date 2019
*/

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


