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

    @file LivelinessMonitor.h
    @author Stan Kladko
    @date 2019
*/

#ifndef SKALED_LIVELINESSMONITOR_H
#define SKALED_LIVELINESSMONITOR_H


#include "MonitoringAgent.h"
#define MONITOR2( _C_, _F_, _T_ )                                                             \
    auto __L__ =                                                                              \
        make_shared< LivelinessMonitor >( getSchain()->getMonitoringAgent(), _C_, _F_, _T_ ); \
    getSchain()->getMonitoringAgent()->registerMonitor( __L__ );

#define MONITOR( _C_, _F_ )                                                                    \
    auto __L__ =                                                                               \
        make_shared< LivelinessMonitor >( getSchain()->getMonitoringAgent(), _C_, _F_, 2000 ); \
    getSchain()->getMonitoringAgent()->registerMonitor( __L__ );


class LivelinessMonitor {
    string cl;
    string function;
    pthread_t threadId = 0;
    uint64_t id = 0;
    uint64_t startTime = 0;
    uint64_t expiryTime = 0;
    static atomic< uint64_t > counter;
    weak_ptr< MonitoringAgent > agent;

public:
    [[nodiscard]] uint64_t getId() const;

    [[nodiscard]] uint64_t getStartTime() const;

    [[nodiscard]] uint64_t getExpiryTime() const;

    string toString();

    virtual ~LivelinessMonitor();

    LivelinessMonitor( const ptr< MonitoringAgent >& _agent, const string& _class,
        const string& _function, uint64_t _maxTime );
};


#endif  // SKALED_LIVELINESSMONITOR_H
