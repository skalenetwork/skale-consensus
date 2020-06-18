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

    @file CatchupWorkerThreadPool.cpp
    @author Stan Kladko
    @date 2018
*/

#include "Agent.h"
#include "CatchupServerAgent.h"
#include "SkaleCommon.h"
#include "Log.h"
#include "abstracttcpserver/AbstractServerAgent.h"
#include "exceptions/FatalError.h"
#include "thirdparty/json.hpp"
#include "threads/WorkerThreadPool.h"


CatchupWorkerThreadPool::CatchupWorkerThreadPool(
        num_threads _numThreads, Agent *_agent) :
        WorkerThreadPool(_numThreads, _agent, false) {


}


void CatchupWorkerThreadPool::createThread(uint64_t threadNumber) {

    auto func = [threadNumber, this](){
        setThreadName("CtchpSrvAg" + to_string(threadNumber), this->agent->getNode()->getConsensusEngine());
        AbstractServerAgent::workerThreadConnectionProcessingLoop((CatchupServerAgent *) agent);
    };

    this->threadpool.push_back(make_shared<thread>(func));
}
