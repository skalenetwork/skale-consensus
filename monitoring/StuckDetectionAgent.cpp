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

    while ( restartTime == 0 ) {
        try {
            usleep( _agent->getSchain()->getNode()->getStuckMonitoringIntervalMs() * 1000 );
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
        _agent->restart( restartTime, restartIteration );
    } catch ( ExitRequestedException& ) {
        return;
    }
}

void StuckDetectionAgent::join() {
    CHECK_STATE( stuckDetectionThreadPool );
    stuckDetectionThreadPool->joinAll();
}

uint64_t StuckDetectionAgent::checkForRestart( uint64_t _restartIteration ) {

    CHECK_STATE( _restartIteration >= 1 );

    auto baseRestartIntervalMs = getSchain()->getNode()->getStuckRestartIntervalMs();

    uint64_t restartIntervalMs = baseRestartIntervalMs * pow( 4, _restartIteration - 1 );

    auto blockID = getSchain()->getLastCommittedBlockID();

    auto currentTimeMs = Time::getCurrentTimeMs();

    // check that the chain has not been doing much for a long time
    if ( getSchain()->getLastCommittedBlockID() > 2 &&
    (currentTimeMs - getSchain()->getStartTimeMs()) > restartIntervalMs &&
    (currentTimeMs - getSchain()->getLastCommitTimeMs() > restartIntervalMs)) {
        auto timeStamp = getSchain()->getBlock( blockID )->getTimeStampS() * 1000;
        // check that nodes are online and do not mine blocks for at least 60 seconds
        while ( Time::getCurrentTimeMs() - currentTimeMs < 60000 ) {
            if ( Time::getCurrentTimeMs() - timeStamp >= restartIntervalMs ) {
                std::unordered_set< uint64_t > connections;
                auto beginTime = Time::getCurrentTimeSec();
                auto nodeCount = getSchain()->getNodeCount();

                // check if can connect to 2/3 of peers. If yes, restart
                while ( 3 * ( connections.size() + 1 ) < 2 * nodeCount ) {
                    if ( Time::getCurrentTimeSec() - beginTime > 10 ) {
                        return 0;  // could not connect to 2/3 of peers
                    }
                }

                for ( int i = 1; i <= nodeCount; i++ ) {
                    if ( i != ( getSchain()->getSchainIndex() ) && !connections.count( i ) ) {
                        try {
                            if ( getNode()->isExitRequested() ) {
                                BOOST_THROW_EXCEPTION( ExitRequestedException( __CLASS_NAME__ ) );
                            }
                            auto socket = make_shared< ClientSocket >(
                                *getSchain(), schain_index( i ), port_type::PROPOSAL );
                            LOG( debug, "Stuck check: connected to peer" );
                            getSchain()->getIo()->writeMagic( socket, true );
                            connections.insert( i );
                        } catch ( ExitRequestedException& ) {
                            throw;
                        } catch ( std::exception& e ) {
                        }
                    }
                }
            }
            // sleep 10 sec before the next check
            usleep( 10000000 );
        }

        cleanupState();
        return timeStamp + restartIntervalMs + 120000;
    }
    return 0;
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
