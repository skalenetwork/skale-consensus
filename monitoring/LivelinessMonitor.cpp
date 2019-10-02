/*
    Copyright (C) 2019 SKALE Labs

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

    @file LivelinessMonitor.cpp
    @author Stan Kladko
    @date 2019
*/
#include "../SkaleCommon.h"
#include "../Log.h"
#include "../thirdparty/json.hpp"

#include "../utils/Time.h"
#include "../chains/Schain.h"
#include "../node/Node.h"
#include "LivelinessMonitor.h"




LivelinessMonitor::LivelinessMonitor(MonitoringAgent *_agent, const char *_class, const char *_function,
                                     uint64_t _maxTime) : cl(_class), function(_function), agent(_agent) {

    CHECK_ARGUMENT(_agent != nullptr);
    CHECK_ARGUMENT(_function != nullptr);
    CHECK_ARGUMENT(_class != nullptr);

    startTime = Time::getCurrentTimeMs();
    expiryTime = startTime + _maxTime;
    threadId = pthread_self();
    agent->registerMonitor(this);

}

LivelinessMonitor::~LivelinessMonitor() {
    agent->unregisterMonitor(this);
}

string LivelinessMonitor::toString() {
    return
    "Node:" + to_string(agent->getSChain()->getNode()->getNodeID()) +
    ":Thread:" + to_string((uint32_t ) threadId) + ":" + cl + string("::") + function;
}

uint64_t LivelinessMonitor::getExpiryTime() const {
    return expiryTime;
}

uint64_t LivelinessMonitor::getStartTime() const {
    return startTime;
}




