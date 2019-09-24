//
// Created by kladko on 24.09.19.
//

#ifndef SKALED_LIVELINESSMONITOR_H
#define SKALED_LIVELINESSMONITOR_H


#define MONITOR(_C_, _F_) LivelinessMonitor __L__(_C_.c_str(), _F_, 3);

class LivelinessMonitor {


    const char* cl;
    const char* function;
    uint64_t  threadId;


    static recursive_mutex mutex;
    static map<uint64_t, LivelinessMonitor*> activeMonitors;

    uint64_t expiryTime;

public:

    virtual ~LivelinessMonitor();

    LivelinessMonitor(const char* _function, const char* _class, uint64_t  _maxTime);





};


#endif //SKALED_LIVELINESSMONITOR_H
