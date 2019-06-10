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
#include "SkaleCommon.h"
#include "node/ConsensusEngine.h"

#include "Consensust.h"

#ifdef GOOGLE_PROFILE
#include <gperftools/heap-profiler.h>
#endif


uint64_t Consensust::getRunningTime() {
    return runningTimeMs;
}

void Consensust::setRunningTime(uint64_t _runningTimeMs) {
    Consensust::runningTimeMs = _runningTimeMs;
}

uint64_t Consensust::runningTimeMs = 60000000;

fs_path Consensust::configDirPath;

const fs_path &Consensust::getConfigDirPath() {
    return configDirPath;
}

void Consensust::setConfigDirPath(const fs_path &_configDirPath) {
    Consensust::configDirPath = _configDirPath;
}



void Consensust::testInit() {

    setConfigDirPath(boost::filesystem::system_complete("."));

#ifdef GOOGLE_PROFILE
    HeapProfilerStart("/tmp/consensusd.profile");
#endif
}

void Consensust::testFinalize() {

    signal(SIGPIPE, SIG_IGN);

#ifdef GOOGLE_PROFILE
    HeapProfilerStop();
#endif
}


/*

TEST_CASE("Consensus init destroy", "[consensus-init-destroy]") {
    Consensust::testInit();

    for (int i = 0; i < 10; i++) {


        INFO("Parsing configs");

        ConsensusEngine engine;


        REQUIRE_NOTHROW(engine.parseConfigsAndCreateAllNodes(Consensust::getConfigDirPath()));


        INFO("Starting nodes");


    }

    Consensust::testFinalize();
}

 */


TEST_CASE("Run basic consensus", "[consensus-basic]") {

    system("/bin/bash -c rm -rf /tmp/*.db");

    Consensust::testInit();

    ConsensusEngine engine;


    INFO("Parsing configs");


    engine.parseConfigsAndCreateAllNodes(Consensust::getConfigDirPath());

    INFO("Starting nodes");


    engine.slowStartBootStrapTest();


    INFO("Running consensus");


    usleep(Consensust::getRunningTime()); /* Flawfinder: ignore */

    assert(engine.nodesCount() > 0);

    assert(engine.getLargestCommittedBlockID() > 0);


    INFO("Exiting gracefully");


    engine.exitGracefully();

    SUCCEED();

    Consensust::testFinalize();

}
