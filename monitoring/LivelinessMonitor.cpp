//
// Created by kladko on 24.09.19.
//

#include "../SkaleCommon.h"
#include "../Log.h"

#include "../chains/Schain.h"
#include "LivelinessMonitor.h"




LivelinessMonitor::LivelinessMonitor(MonitoringAgent *_agent, const char *_function, const char *_class,
                                     uint64_t _maxTime) : agent(_agent), function(_function),
                                             cl(_class) {

    CHECK_ARGUMENT(_agent != nullptr);
    CHECK_ARGUMENT(_function != nullptr);
    CHECK_ARGUMENT(_class != nullptr);

    startTime = Schain::getCurrentTimeMs();
    expiryTime = startTime + _maxTime;
    threadId = pthread_self();
    agent->registerMonitor(this);

}

LivelinessMonitor::~LivelinessMonitor() {
    agent->unregisterMonitor(this);
}

string LivelinessMonitor::toString() {
    return "Thread" + to_string(threadId) + ":" + cl + string("::") + function;
}

uint64_t LivelinessMonitor::getExpiryTime() const {
    return expiryTime;
}

uint64_t LivelinessMonitor::getStartTime() const {
    return startTime;
}




