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

#include "../utils/Time.h"
#include "../node/Node.h"
#include "../chains/Schain.h"
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

    lock_guard<recursive_mutex> lock(mutex);

    for (auto &&item: activeMonitors) {
        LivelinessMonitor *m = item.second;

        CHECK_STATE(m != nullptr);

        auto currentTime = Time::getCurrentTimeMs();

        if (currentTime > m->getExpiryTime()) {
            LOG(warn, m->toString() + " has been stuck for " + to_string(currentTime - m->getStartTime()) + " ms");
        }

    }
}

void MonitoringAgent::monitoringLoop(MonitoringAgent *agent) {
    setThreadName("MonitoringLoop");


    LOG(info, "Monitoring agent started monitoring");


    try {
        while (!agent->getSChain()->getNode()->isExitRequested()) {
            usleep(agent->getSChain()->getNode()->getMonitoringIntervalMs() * 1000);

            try {
                agent->monitor();
            } catch (ExitRequestedException &) {
                return;
            } catch (Exception &e) {
                Exception::logNested(e);
            }

        };
    } catch (FatalError *e) {
        agent->getSChain()->getNode()->exitOnFatalError(e->getMessage());
    }
}

void MonitoringAgent::registerMonitor(LivelinessMonitor *m) {

    CHECK_ARGUMENT(m != nullptr)

    lock_guard<recursive_mutex> lock(mutex);

    activeMonitors[(uint64_t) m] = m;

}

void MonitoringAgent::unregisterMonitor(LivelinessMonitor *m) {

    CHECK_ARGUMENT(m != nullptr);

    lock_guard<recursive_mutex> lock(mutex);

    activeMonitors.erase((uint64_t) m);

}

Schain *MonitoringAgent::getSChain() const {
    return sChain;
}

void MonitoringAgent::join() {
    this->monitoringThreadPool->joinAll();
}
