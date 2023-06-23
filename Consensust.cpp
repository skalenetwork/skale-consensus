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

    @file Skaled.cpp
    @author Stan Kladko
    @date 2018
*/

#define CATCH_CONFIG_MAIN

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>


#include <cryptopp/eccrypto.h>
#include <cryptopp/osrng.h>
#include <cryptopp/sha.h>
#include <cryptopp/oids.h>
#include <cryptopp/hex.h>


#include "thirdparty/catch.hpp"

#include "SkaleCommon.h"
#include "Log.h"
#include "crypto/CryptoManager.h"
#include "node/ConsensusEngine.h"

#include "iostream"
#include "time.h"
#include "crypto/BLAKE3Hash.h"

#include "json/JSONFactory.h"
#include "utils/Time.h"

#include "Consensust.h"
#include "JsonStubClient.h"
#include <network/Utils.h>

#ifdef GOOGLE_PROFILE
#include <gperftools/heap-profiler.h>
#endif


ConsensusEngine* engine;


class DontCleanup {
public:
    DontCleanup() { Consensust::setConfigDirPath( boost::filesystem::system_complete( "." ) ); };

    ~DontCleanup() {}
};


class StartFromScratch {
public:
    StartFromScratch() {
        int i = system( "rm -rf /tmp/*.db.*" );
        i = system( "rm -rf /tmp/*.db" );
        i++;  // make compiler happy
        Consensust::setConfigDirPath( boost::filesystem::system_complete( "." ) );

#ifdef GOOGLE_PROFILE
        HeapProfilerStart( "/tmp/consensusd.profile" );
        HeapProfilerStart( "/tmp/consensusd.profile" );
#endif
    };

    ~StartFromScratch() {
#ifdef GOOGLE_PROFILE
        HeapProfilerStop();
#endif
    }
};

uint64_t Consensust::getRunningTimeS() {
    if ( runningTimeS == 0 ) {
        auto env = getenv( "TEST_TIME_S" );

        if ( env != NULL ) {
            runningTimeS = strtoul( env, NULL, 10 );
        } else {
            runningTimeS = DEFAULT_RUNNING_TIME_S;
        }
    }

    return runningTimeS;
}

uint64_t Consensust::runningTimeS = 0;

fs_path Consensust::configDirPath;

const fs_path& Consensust::getConfigDirPath() {
    return configDirPath;
}


void Consensust::setConfigDirPath( const fs_path& _configDirPath ) {
    Consensust::configDirPath = _configDirPath;
}

void Consensust::useCorruptConfigs() {
    configDirPath += "/corrupt";
}


void testLog( const char* message ) {
    printf( "TEST_LOG: %s\n", message );
}

void abort_handler( int ) {
    printf( "cought SIGABRT, exiting.\n" );
    exit( 0 );
}

block_id basicRun( int64_t _lastId = 0 ) {
    try {
        REQUIRE( ConsensusEngine::getEngineVersion().size() > 0 );

        engine = new ConsensusEngine( _lastId, 1000000000 );


        engine->parseTestConfigsAndCreateAllNodes( Consensust::getConfigDirPath(), _lastId == -1 );


        engine->slowStartBootStrapTest();


        auto startTime = Time::getCurrentTimeSec();

        while ( Time::getCurrentTimeSec() < startTime + Consensust::getRunningTimeS() ) {
            try {
                usleep( 1000 * 1000 );
            } catch ( ... ) {
            };
        }

        REQUIRE( engine->nodesCount() > 0 );
        auto lastId = engine->getLargestCommittedBlockID();
        REQUIRE( lastId > 0 );

        auto [transactions, timestampS, timeStampMs, price, stateRoot] = engine->getBlock( 1 );


        REQUIRE( transactions );
        REQUIRE( timestampS > 0 );

        cerr << price << ":" << stateRoot << endl;
        signal( SIGABRT, abort_handler );
        engine->exitGracefully();

        while ( engine->getStatus() != CONSENSUS_EXITED ) {
            usleep( 100 * 1000 );
        }

        delete engine;
        return lastId;
    } catch ( SkaleException& e ) {
        SkaleException::logNested( e );
        throw;
    }
}


bool success = false;

void exit_check() {
    sleep( STUCK_TEST_TIME );
    engine->exitGracefully();

    while ( engine->getStatus() != CONSENSUS_EXITED ) {
        usleep( 100 * 1000 );
    }
}


#include "unittests/consensus_tests.cpp"
#include "unittests/sgx_tests.cpp"
