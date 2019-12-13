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

#include <node/ConsensusEngine.h>
#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/FatalError.h"
#include "thirdparty/json.hpp"

#include "utils/Time.h"
#include "node/Node.h"
#include "chains/Schain.h"
#include "LivelinessMonitor.h"
#include "MonitoringAgent.h"
#include "MonitoringThreadPool.h"


MonitoringAgent::MonitoringAgent(Schain &_sChain) {
    try {
        logThreadLocal_ = _sChain.getNode()->getLog();
        this->sChain = &_sChain;



        this->monitoringThreadPool = make_shared<MonitoringThreadPool>(1, this);
        monitoringThreadPool->startService();

    } catch (...) {
        throw_with_nested(FatalError(__FUNCTION__, __CLASS_NAME__));
    }

}


void MonitoringAgent::monitor() {

    if (ConsensusEngine::isOnTravis())
        return;

    lock_guard<recursive_mutex> lock(m);

    for (auto &&item: activeMonitors) {
        LivelinessMonitor *monitor = item.second;

        CHECK_STATE(monitor != nullptr);

        auto currentTime = Time::getCurrentTimeMs();

        if (currentTime > monitor->getExpiryTime()) {
            LOG(warn, monitor->toString() + " has been stuck for " + to_string(currentTime - monitor->getStartTime()) + " ms");
        }

    }
}

void MonitoringAgent::monitoringLoop(MonitoringAgent *agent) {
    setThreadName("MonitoringLoop", agent->getSchain()->getNode()->getConsensusEngine());


    LOG(info, "Monitoring agent started monitoring");


    try {
        while (!agent->getSchain()->getNode()->isExitRequested()) {
            usleep(agent->getSchain()->getNode()->getMonitoringIntervalMs() * 1000);

            try {
                agent->monitor();
            } catch (ExitRequestedException &) {
                return;
            } catch (exception &e) {
                Exception::logNested(e);
            }

        };
    } catch (FatalError *e) {
        agent->getSchain()->getNode()->exitOnFatalError(e->getMessage());
    }
}

void MonitoringAgent::registerMonitor(LivelinessMonitor *_monitor) {

    CHECK_ARGUMENT(_monitor != nullptr)

    lock_guard<recursive_mutex> lock(m);

    activeMonitors[(uint64_t) _monitor] = _monitor;

}

void MonitoringAgent::unregisterMonitor(LivelinessMonitor *_monitor) {

    CHECK_ARGUMENT(_monitor != nullptr);

    lock_guard<recursive_mutex> lock(m);

    activeMonitors.erase((uint64_t) _monitor);

}

Schain *MonitoringAgent::getSchain() const {
    return sChain;
}

void MonitoringAgent::join() {
    this->monitoringThreadPool->joinAll();
}
