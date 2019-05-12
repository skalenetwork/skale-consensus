/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skale-consensus.  If not, see <http://www.gnu.org/licenses/>.

    @file Skaled.cpp
    @author Stan Kladko
    @date 2018
*/

#define CATCH_CONFIG_MAIN
#include "thirdparty/catch.hpp"
#include "Log.h"
#include "SkaleConfig.h"
#include "node/ConsensusEngine.h"

#include "Consensust.h"

#ifdef GOOGLE_PROFILE
#include <gperftools/heap-profiler.h>
#endif



uint64_t Consensust::getRunningTime()  {
    return runningTimeMs;
}

void Consensust::setRunningTime(uint64_t _runningTimeMs) {
    Consensust::runningTimeMs = _runningTimeMs;
}

uint64_t Consensust::runningTimeMs = 60000000;



TEST_CASE( "Run basic consensus", "[basic-consensus]") {

#ifdef GOOGLE_PROFILE
    HeapProfilerStart("/tmp/consensusd.profile");
#endif

    signal(SIGPIPE, SIG_IGN);


    fs_path dirPath(boost::filesystem::system_complete("."));

    ConsensusEngine engine;


    INFO("Parsing configs");


    REQUIRE_NOTHROW(engine.parseConfigsAndCreateAllNodes(dirPath));


    INFO("Starting nodes");


    REQUIRE_NOTHROW(engine.slowStartBootStrapTest());


    INFO("Running consensus");


    usleep(Consensust::getRunningTime());


    INFO("Exiting gracefully");


    REQUIRE_NOTHROW(engine.exitGracefully());

#ifdef GOOGLE_PROFILE
    HeapProfilerStop();
#endif


    SUCCEED();

}
