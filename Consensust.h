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

    @file ConsensusTest.h
    @author Stan Kladko
    @date 2018
*/

#pragma once

#include "SkaleCommon.h"
#include "node/ConsensusEngine.h"

#define DEFAULT_RUNNING_TIME_S 30
#define STUCK_TEST_TIME 5

extern ConsensusEngine* engine;

class Consensust {
    static uint64_t runningTimeS;
    static fs_path configDirPath;

public:
    static const fs_path& getConfigDirPath();

    static void setConfigDirPath( const fs_path& _configDirPath );

    static void useCorruptConfigs();

    static uint64_t getRunningTimeS();


    static void testInit();

    static void testFinalize();
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

class DontCleanup {
public:
    DontCleanup() { Consensust::setConfigDirPath( boost::filesystem::system_complete( "." ) ); };

    ~DontCleanup() {}
};

block_id basicRun( int64_t _lastId = 0 );
void exit_check();
void abort_handler( int );
void testLog( const char* message );