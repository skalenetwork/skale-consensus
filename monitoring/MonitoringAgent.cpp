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

        this->monitoringThreadPool = make_shared<MonitoringThreadPool>(1, this);
        monitoringThreadPool->startService();
    } catch (...) {
        throw_with_nested(FatalError(__FUNCTION__, __CLASS_NAME__));
    }

}


void MonitoringAgent::monitor() {
}


void MonitoringAgent::monitoringLoop(MonitoringAgent *agent) {
    setThreadName(__CLASS_NAME__);

    agent->waitOnGlobalStartBarrier();

    try {
        while (!agent->getSchain()->getNode()->isExitRequested()) {
            usleep(agent->getNode()->getMonitoringIntervalMs() * 1000);

            try {
                agent->monitor();
            } catch (ExitRequestedException &) {
                return;
            } catch (Exception &e) {
                Exception::logNested(e);
            }

        };
    } catch (FatalError *e) {
        agent->getNode()->exitOnFatalError(e->getMessage());
    }
}
