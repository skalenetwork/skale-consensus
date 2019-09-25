//
// Created by kladko on 24.09.19.
//

#ifndef SKALED_LIVELINESSMONITOR_H
#define SKALED_LIVELINESSMONITOR_H


#include "MonitoringAgent.h"

#define MONITOR(_C_, _F_) LivelinessMonitor __L__(getSchain()->getMonitoringAgent().get(), _C_.c_str(), _F_, 0);

class LivelinessMonitor {


    const char* cl;
    const char* function;
    uint64_t  threadId;

    MonitoringAgent* agent = nullptr;
    uint64_t startTime;
public:
    uint64_t getStartTime() const;

private:
    uint64_t expiryTime;
public:
    uint64_t getExpiryTime() const;

public:

    string toString();

    void monitor();

    virtual ~LivelinessMonitor();

    LivelinessMonitor(MonitoringAgent *_agent, const char *_function, const char *_class, uint64_t _maxTime);







};


#endif //SKALED_LIVELINESSMONITOR_H
