/*
    Copyright (C) 2018-2019 SKALE Labs

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

    @file WorkerThreadPool.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../SkaleCommon.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"


#include "WorkerThreadPool.h"


vector<ptr<thread>> WorkerThreadPool::allThreads;

void WorkerThreadPool::startService() {

    for (uint64_t i = 0; i < (uint64_t )numThreads; i++) {
        createThread(i);
        allThreads.push_back(threadpool.at(i));
    }

}


WorkerThreadPool::WorkerThreadPool(num_threads _numThreads, void* _param) {
   assert(_numThreads > 0);
   LOG(trace, "Started threads count:" + to_string(_numThreads));
   this->params = _param;
   this->numThreads = _numThreads;
}

const vector<shared_ptr<thread>> &WorkerThreadPool::getAllThreads() {
    return allThreads;
}

void WorkerThreadPool::addThread(ptr<thread> _t) {
    allThreads.push_back(_t);
}
