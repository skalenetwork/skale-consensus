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

    @file StuckDetectionAgent.cpp
    @author Stan Kladko
    @date 2021
*/

#include "Log.h"
#include "SkaleCommon.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/FatalError.h"
#include "thirdparty/json.hpp"
#include <node/ConsensusEngine.h>

#include "LivelinessMonitor.h"
#include "StuckDetectionAgent.h"
#include "StuckDetectionThreadPool.h"
#include "chains/Schain.h"
#include "node/Node.h"
#include "utils/Time.h"

#include "utils/Time.h"

StuckDetectionAgent::StuckDetectionAgent( Schain& _sChain ) : Agent( _sChain, false, true ) {
    try {
        logThreadLocal_ = _sChain.getNode()->getLog();
        this->sChain = &_sChain;
        this->stuckDetectionThreadPool = make_shared< StuckDetectionThreadPool >( 1, this );
        stuckDetectionThreadPool->startService();

    } catch ( ... ) {
        throw_with_nested( FatalError( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


void StuckDetectionAgent::StuckDetectionLoop( StuckDetectionAgent* _agent ) {
    CHECK_ARGUMENT( _agent );

    setThreadName( "StuckDetectionLoop", _agent->getSchain()->getNode()->getConsensusEngine() );

    _agent->getSchain()->getSchain()->waitOnGlobalStartBarrier();

    LOG( info, "StuckDetection agent started monitoring" );

    try {
        while ( true ) {
            auto restartTime = _agent->checkForRestart();

            if ( restartTime > 0 ) {
                _agent->restart( restartTime );
            }

        usleep(_agent->getSchain()->getNode()->getStuckMonitoringIntervalMs() * 1000);
        }

    } catch ( ExitRequestedException& ) {
        return;
    } catch ( exception& e ) {
        SkaleException::logNested( e );
    }
}

void StuckDetectionAgent::join() {
    CHECK_STATE( stuckDetectionThreadPool );
    stuckDetectionThreadPool->joinAll();
}

uint64_t StuckDetectionAgent::checkForRestart() {
    return Time::getCurrentTimeMs() + 10000;
}
void StuckDetectionAgent::restart( uint64_t _timeMs) {
    usleep(_timeMs);
    exit(13);
}
