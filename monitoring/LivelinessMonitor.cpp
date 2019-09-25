//
// Created by kladko on 24.09.19.
//

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
    "Node:" + to_string(agent->getNode()->getNodeID()) +
    ":Thread:" + to_string((uint32_t ) threadId) + ":" + cl + string("::") + function;
}

uint64_t LivelinessMonitor::getExpiryTime() const {
    return expiryTime;
}

uint64_t LivelinessMonitor::getStartTime() const {
    return startTime;
}




