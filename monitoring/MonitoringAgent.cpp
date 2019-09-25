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

    @file MonitoringAgent.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../../SkaleCommon.h"

#include "../../Log.h"
#include "../../exceptions/ExitRequestedException.h"
#include "../../exceptions/FatalError.h"

#include "../../thirdparty/json.hpp"

#include "../../node/Node.h"

#include "../../chains/Schain.h"

#include "../../exceptions/NetworkProtocolException.h"

#include "MonitoringAgent.h"
#include "MonitoringThreadPool.h"


MonitoringAgent::MonitoringAgent(Schain &_sChain) : Agent(_sChain, true) {
    try {
        logThreadLocal_ = _sChain.getNode()->getLog();
        this->sChain = &_sChain;
        threadCounter = 0;

        this->MonitoringThreadPool = make_shared<MonitoringThreadPool>(1, this);
        MonitoringThreadPool->startService();
    }
} catch (...) {
    throw_with_nested(FatalError(__FUNCTION__, __CLASS_NAME__));
}

}



void MonitoringAgent::sync(schain_index _dstIndex) {
}




void MonitoringAgent::monitoringLoop(MonitoringAgent *agent)  {
    setThreadName(__CLASS_NAME__);

    agent->waitOnGlobalStartBarrier();

    try {
        while (!agent->getSchain()->getNode()->isExitRequested()) {
            usleep(agent->getNode()->getCatchupIntervalMs() * 1000);

            try {
                agent->sync(destinationSubChainIndex);
            } catch (ExitRequestedException &) {
                return;
            } catch (Exception &e) {
                Exception::logNested(e);
            }

            destinationSubChainIndex = nextSyncNodeIndex(agent, destinationSubChainIndex);
        };
    } catch (FatalError *e) {
        agent->getNode()->exitOnFatalError(e->getMessage());
    }
}

schain_index MonitoringAgent::nextSyncNodeIndex(const MonitoringAgent *agent, schain_index _destinationSubChainIndex) {
    auto nodeCount = (uint64_t) agent->getSchain()->getNodeCount();

    auto index = _destinationSubChainIndex - 1;

    do {
        index = ((uint64_t) index + 1) % nodeCount;
    } while (index == (agent->getSchain()->getSchainIndex() - 1));

    return index + 1;
}
