/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with skale-consensus.  If not, see <https://www.gnu.org/licenses/>.

    @file WorkerThreadPool.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"


#include "GlobalThreadRegistry.h"
#include "WorkerThreadPool.h"


void WorkerThreadPool::startService() {

    ASSERT(!started)

    LOCK(m)

    started = true;

    for (uint64_t i = 0; i < (uint64_t) numThreads; i++) {
        createThread(i);
        if (!dontJoinGlobalRegistry)
            agent->getThreadRegistry()->add(threadpool.at(i));
    }

}


WorkerThreadPool::WorkerThreadPool(num_threads _numThreads, Agent *_agent, bool _dontJoinGlobalRegistry) {
    CHECK_ARGUMENT(_numThreads > 0);
    CHECK_ARGUMENT(_agent != nullptr);
    LOG(trace, "Started threads count:" + to_string(_numThreads));
    this->dontJoinGlobalRegistry = _dontJoinGlobalRegistry;
    this->agent = _agent;
    this->numThreads = _numThreads;;
}


void WorkerThreadPool::joinAll() {
    LOCK(m)

    if (joined)
        return;

    joined = true;

    for (auto &&thread : threadpool) {
        CHECK_STATE(thread->joinable());
        thread->join();
        CHECK_STATE(!thread->joinable());
    }
}

bool WorkerThreadPool::isJoined() const {
    return joined;
}

WorkerThreadPool::~WorkerThreadPool(){
    threadpool.clear();
}
