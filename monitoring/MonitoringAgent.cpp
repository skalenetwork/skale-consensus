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

#include "Log.h"
#include "SkaleCommon.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/FatalError.h"
#include "thirdparty/json.hpp"
#include <node/ConsensusEngine.h>

#include "LivelinessMonitor.h"
#include "MonitoringAgent.h"
#include "MonitoringThreadPool.h"
#include "chains/Schain.h"
#include "node/Node.h"
#include "utils/Time.h"

#include "utils/Time.h"

MonitoringAgent::MonitoringAgent( Schain& _sChain ) : Agent( _sChain, false, true ) {
    try {
        this->sChain = &_sChain;

        this->monitoringThreadPool = make_shared< MonitoringThreadPool >( 1, this );
        monitoringThreadPool->startService();

    } catch ( ... ) {
        throw_with_nested( FatalError( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


void MonitoringAgent::monitor() {
    if ( ConsensusEngine::isOnTravis() )
        return;

    while ( !getNode()->isInited() ) {
        usleep( 100000 );
    }

    // While consensus is paused (e.g. via debug_pauseConsensus), block-processing
    // threads intentionally sit idle past their normal liveliness timeouts, so
    // "stuck" warnings would just be noise. Skip reporting until unpaused.
    if ( getNode()->isPaused() )
        return;

    map< uint64_t, weak_ptr< LivelinessMonitor > > monitorsCopy;

    {
        LOCK( monitorsMutex )
        monitorsCopy = activeMonitors;
    }

    for ( auto&& item : monitorsCopy ) {
        if ( sChain->getNode()->isExitRequested() )
            return;

        ptr< LivelinessMonitor > monitor = item.second.lock();

        if ( monitor ) {
            CHECK_STATE( monitor != nullptr );

            auto currentTime = Time::getCurrentTimeMs();

            if ( currentTime > monitor->getExpiryTime() ) {
                CONS_LOG( warn, monitor->toString()
                               << " has been stuck for "
                               << to_string( currentTime - monitor->getStartTime() ) + " ms" );
            }
        }
    }
}


void MonitoringAgent::monitoringLoop( MonitoringAgent* _agent ) {
    CHECK_ARGUMENT( _agent );

    logThreadLocal_ = _agent->getSchain()->getNode()->getLog();
    setThreadName( "MonitoringLoop", _agent->getSchain()->getNode()->getConsensusEngine() );


    CONS_LOG( info, "Monitoring agent started monitoring" );

    try {
        auto intervalMs = _agent->getSchain()->getNode()->getMonitoringIntervalMs();

        while ( true ) {
            {
                std::unique_lock< std::mutex > lock( _agent->stopMutex );
                _agent->stopCond.wait_for( lock, std::chrono::milliseconds( intervalMs ),
                    [_agent] { return _agent->stopRequested.load(); } );

                // In test, we set the condition variable to exit,
                // thus we exit the loop from this condition
                if ( _agent->stopRequested.load() ) {
                    return;
                }
            }

            // In production, we do not set the condition variable to exit.
            // So it will wake up after the interval, and will just check if the node is exiting.
            if ( _agent->getSchain()->getNode()->isExitRequested() ) {
                return;
            }

            try {
                _agent->monitor();

            } catch ( ExitRequestedException& ) {
                return;
            } catch ( exception& e ) {
                SkaleException::logNested( e );
            }
        }
    } catch ( FatalError& e ) {
        SkaleException::logNested( e );
        _agent->getSchain()->getNode()->initiateApplicationExitOnFatalConsensusError( e.what() );
    }
}

void MonitoringAgent::registerMonitor( const ptr< LivelinessMonitor >& _m ) {
    CHECK_ARGUMENT( _m )
    LOCK( monitorsMutex )
    activeMonitors[_m->getId()] = _m;
}

void MonitoringAgent::unregisterMonitor( uint64_t _id ) {
    LOCK( monitorsMutex )
    activeMonitors.erase( _id );
}

void MonitoringAgent::stop() {
    {
        std::lock_guard< std::mutex > lock( stopMutex );
        stopRequested = true;
    }
    stopCond.notify_all();
}

void MonitoringAgent::join() {
    CHECK_STATE( monitoringThreadPool );
    monitoringThreadPool->joinAll();
}
