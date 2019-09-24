//
// Created by kladko on 24.09.19.
//

#include "../SkaleCommon.h"
#include "../Log.h"

#include "LivelinessMonitor.h"




LivelinessMonitor::LivelinessMonitor(const char* _function, const char* _class, uint64_t  _maxTime) {

    function = _function;
    cl = _class;
    expiryTime = time(NULL) + _maxTime;
    threadId = pthread_self();

    lock_guard<recursive_mutex> lock(mutex);

    activeMonitors[(uint64_t) this] = this;
}

LivelinessMonitor::~LivelinessMonitor() {

    lock_guard<recursive_mutex> lock(mutex);

    activeMonitors.erase((uint64_t) this);
}

recursive_mutex LivelinessMonitor::mutex;
map<uint64_t, LivelinessMonitor*> LivelinessMonitor::activeMonitors;
