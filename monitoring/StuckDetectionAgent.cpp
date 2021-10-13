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
#include "datastructures/CommittedBlock.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/FatalError.h"
#include "thirdparty/json.hpp"
#include <db/BlockSigShareDB.h>
#include <db/ConsensusStateDB.h>
#include <db/DAProofDB.h>
#include <db/DASigShareDB.h>
#include <db/MsgDB.h>
#include <db/ProposalVectorDB.h>
#include <db/RandomDB.h>
#include <network/ClientSocket.h>
#include <network/IO.h>
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
        // we only need one agent
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


    uint64_t restartIteration = 1;

    while ( true ) {
        auto restartFileName = _agent->createStuckFileName( restartIteration );

        if ( !boost::filesystem::exists( restartFileName ) ) {
            break;
        }
        restartIteration++;
        CHECK_STATE( restartIteration < 64 );
    }

    uint64_t restartTime = 0;
    uint64_t sleepTime = _agent->getSchain()->getNode()->getStuckMonitoringIntervalMs() * 1000;

    while ( restartTime == 0 ) {
        try {
            usleep( sleepTime );
            _agent->getSchain()->getNode()->exitCheck();
            restartTime = _agent->checkForRestart( restartIteration );
        } catch ( ExitRequestedException& ) {
            return;
        } catch ( exception& e ) {
            SkaleException::logNested( e );
        }
    }


    CHECK_STATE( restartTime > 0 );
    try {
        LOG( info, "Restarting node because of stuck detected." );
        _agent->restart( restartTime, restartIteration );
    } catch ( ExitRequestedException& ) {
        return;
    }
}

void StuckDetectionAgent::join() {
    CHECK_STATE( stuckDetectionThreadPool );
    stuckDetectionThreadPool->joinAll();
}


bool StuckDetectionAgent::checkNodesAreOnline() {

    LOG( info, "Stuck detected. Checking network connectivity ..." );

    std::unordered_set< uint64_t > connections;
    auto beginTime = Time::getCurrentTimeSec();
    auto nodeCount = getSchain()->getNodeCount();

    // check if can connect to 2/3 of peers. If yes, restart
    while ( 3 * ( connections.size() + 1 ) < 2 * nodeCount ) {
        if ( Time::getCurrentTimeSec() - beginTime > 10 ) {
            LOG( info, "Could not connect to 2/3 of nodes. Will not restart" );
            return false;  // could not connect to 2/3 of peers
        }

        for ( int i = 1; i <= nodeCount; i++ ) {
            if ( i != ( getSchain()->getSchainIndex() ) && !connections.count( i ) ) {
                try {
                    if ( getNode()->isExitRequested() ) {
                        BOOST_THROW_EXCEPTION( ExitRequestedException( __CLASS_NAME__ ) );
                    }
                    LOG( info, "Stuck check: connecting to peer:" + to_string( i ) );
                    auto socket = make_shared< ClientSocket >(
                        *getSchain(), schain_index( i ), port_type::PROPOSAL );
                    LOG( info, "Stuck check: connected to peer:" + to_string( i ) );
                    getSchain()->getIo()->writeMagic( socket, true );
                    connections.insert( i );
                } catch ( ExitRequestedException& ) {
                    throw;
                } catch ( std::exception& e ) {
                    LOG( info, "Stuck check: could not connect to peer:" + to_string( i ) );
                }
            }
        }
    }
    LOG( info, "Could connect to 2/3 of nodes" );
    return true;
}


bool StuckDetectionAgent::stuckCheck( uint64_t _restartIntervalMs, uint64_t _timeStamp ) {
    auto currentTimeMs = Time::getCurrentTimeMs();

    auto result = ( currentTimeMs - getSchain()->getStartTimeMs() ) > _restartIntervalMs &&
                  ( currentTimeMs - getSchain()->getLastCommitTimeMs() > _restartIntervalMs ) &&
                  ( Time::getCurrentTimeMs() - _timeStamp > _restartIntervalMs ) &&
                  checkNodesAreOnline();



    return result;
}

uint64_t StuckDetectionAgent::checkForRestart( uint64_t _restartIteration ) {
    CHECK_STATE( _restartIteration >= 1 );

    auto baseRestartIntervalMs = getSchain()->getNode()->getStuckRestartIntervalMs();

    uint64_t restartIntervalMs = baseRestartIntervalMs * pow( 4, _restartIteration - 1 );

    auto blockID = getSchain()->getLastCommittedBlockID();

    if ( blockID < 2 )
        return 0;

    auto timeStampMs = getSchain()->getBlock( blockID )->getTimeStampS() * 1000;

    // check that the chain has not been doing much for a long time
    auto startTimeMs = Time::getCurrentTimeMs();
    while (Time::getCurrentTimeMs() - startTimeMs < 60000 ) {
        if ( !stuckCheck( restartIntervalMs, timeStampMs ) )
            return 0;
        usleep(5 * 1000 * 1000);
    }

    LOG( info, "Need for restart detected. Cleaning and restarting " );
    cleanupState();

    LOG( info, "Cleaned up state" );

    return timeStampMs + restartIntervalMs + 120000;
}
void StuckDetectionAgent::restart( uint64_t _restartTimeMs, uint64_t _iteration ) {
    CHECK_STATE( _restartTimeMs > 0 );


    while ( Time::getCurrentTimeMs() < _restartTimeMs ) {
        try {
            usleep( 100 );
        } catch ( ... ) {
        }

        getNode()->exitCheck();
    }

    createStuckRestartFile( _iteration + 1 );

    exit( 13 );
}

string StuckDetectionAgent::createStuckFileName( uint64_t _iteration ) {
    CHECK_STATE( _iteration >= 1 );
    auto engine = getNode()->getConsensusEngine();
    CHECK_STATE( engine );
    string fileName = engine->getHealthCheckDir() + "/STUCK_RESTART";
    fileName.append( "." + to_string( getNode()->getNodeID() ) );
    fileName.append( "." + to_string( sChain->getLastCommittedBlockID() ) );
    fileName.append( "." + to_string( _iteration ) );
    return fileName;
};

void StuckDetectionAgent::createStuckRestartFile( uint64_t _iteration ) {
    CHECK_STATE( _iteration >= 1 );
    auto fileName = createStuckFileName( _iteration );

    ofstream f;
    f.open( fileName, ios::trunc );
    f << " ";
    f.close();
}

void StuckDetectionAgent::cleanupState() {
    getSchain()->getNode()->getIncomingMsgDB()->destroy();
    getSchain()->getNode()->getOutgoingMsgDB()->destroy();
    getSchain()->getNode()->getBlockSigShareDB()->destroy();
    getSchain()->getNode()->getDaSigShareDB()->destroy();
    getSchain()->getNode()->getDaProofDB()->destroy();
    getSchain()->getNode()->getConsensusStateDB()->destroy();
    getSchain()->getNode()->getProposalVectorDB()->destroy();
    getSchain()->getNode()->getRandomDB()->destroy();
}
