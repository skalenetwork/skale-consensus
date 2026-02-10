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

    @file TimeoutAgent.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/FatalError.h"
#include "thirdparty/json.hpp"
#include <node/ConsensusEngine.h>

#include "utils/Time.h"
#include "node/Node.h"
#include "chains/Schain.h"
#include "LivelinessMonitor.h"
#include "TimeoutAgent.h"
#include "TimeoutThreadPool.h"

#include "utils/Time.h"

TimeoutAgent::TimeoutAgent( Schain& _sChain ) : Agent( _sChain, false, true ) {
    try {
        this->sChain = &_sChain;
        this->timeoutThreadPool = make_shared< TimeoutThreadPool >( 1, this );
        timeoutThreadPool->startService();

    } catch ( ... ) {
        throw_with_nested( FatalError( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


void TimeoutAgent::timeoutLoop() {

    setThreadName( "TimeoutLoop", getSchain()->getNode()->getConsensusEngine() );

    getSchain()->getSchain()->waitOnGlobalStartBarrier();
    
    logThreadLocal_ = getSchain()->getNode()->getLog();

    if ( getSchain()->getNode()->isExitRequested() )
        return;

    CONS_LOG( info, "Timeout agent started monitoring" );

    uint64_t blockProcessingStart =
        max( getSchain()->getLastCommitTimeMs(), getSchain()->getStartTimeMs() );

    if ( blockProcessingStart == 0 )
        blockProcessingStart = Time::getCurrentTimeMs();

    uint64_t lastRebroadCastTime = blockProcessingStart;

    bool proposalReceiptTimedOut = false;

    try {
        while ( !getSchain()->getNode()->isExitRequested() ) {
            usleep( getSchain()->getNode()->getMonitoringIntervalMs() * 1000 );

            try {
                auto currentBlockId = getSchain()->getLastCommittedBlockID() + 1;
                auto currentTime = Time::getCurrentTimeMs();

                auto timeZero = max( getSchain()->getLastCommitTimeMs(),
                    getSchain()->getStartTimeMs() );
                blockProcessingStart = timeZero;

                lastRebroadCastTime = max( lastRebroadCastTime, timeZero );

                // If early timeout is forced, trigger timeout event immediately without waiting 
                // for the normal timeout.
                if ( earlyTimeoutForced ) {
                    try {
                        getSchain()->blockProposalReceiptTimeoutArrived(
                            currentBlockId );
                        earlyTimeoutForced = false;
                    } catch ( ... ) {
                    }
                }

                if ( getSchain()->getNodeCount() > 2 ) {
                    if ( currentTime - blockProcessingStart <= BLOCK_PROPOSAL_RECEIVE_TIMEOUT_MS )
                        proposalReceiptTimedOut = false;


                    if ( !proposalReceiptTimedOut &&
#ifndef BITE
                         currentBlockId > 2 &&
#endif
                         currentTime - blockProcessingStart > BLOCK_PROPOSAL_RECEIVE_TIMEOUT_MS ) {
                        try {
                            getSchain()->blockProposalReceiptTimeoutArrived(
                                currentBlockId );
                            proposalReceiptTimedOut = true;
                        } catch ( ... ) {
                        }
                    }

                    if ( currentBlockId > 2 &&
                         currentTime - lastRebroadCastTime > REBROADCAST_TIMEOUT_MS ) {
                        getSchain()->rebroadcastAllMessagesForCurrentBlock();
                        lastRebroadCastTime = currentTime;
                    }
                }
            } catch ( ExitRequestedException& ) {
                return;
            } catch ( exception& e ) {
                SkaleException::logNested( e );
            }
        };
    } catch ( FatalError& e ) {
        SkaleException::logNested( e );
        getSchain()->getNode()->initiateApplicationExitOnFatalConsensusError( e.what() );
    }
}

void TimeoutAgent::forceEarlyTimeout() {
    earlyTimeoutForced = true;
}

void TimeoutAgent::join() {
    CHECK_STATE( timeoutThreadPool );
    timeoutThreadPool->joinAll();
}
