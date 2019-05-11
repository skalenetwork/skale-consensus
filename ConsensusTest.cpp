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

//#include <folly/File.h>
#include "SkaleConfig.h"

#include "node/ConsensusEngine.h"

void runTest(char *_testDir, const shared_ptr<set<node_id>> &_nodeIdstoRun);

#ifdef GOOGLE_PROFILE
#include <gperftools/heap-profiler.h>
#endif


int main(int argc, char **argv) {

#ifdef GOOGLE_PROFILE
    HeapProfilerStart("/tmp/consensustest.profile");
#endif



    if (argc < 2) {
        printf("Usage: consensustest nodes_dir node_id1 node_id2 \n");
        exit(1);
    }

    auto nodeIdstoRun = make_shared<set<node_id>>();

    for (int i = 2; i < argc; i++) {

        uint64_t ui64;
        ui64 = static_cast<uint64_t >(stoll(argv[i]));

        nodeIdstoRun->insert(node_id(ui64));
        cerr << node_id(ui64) << endl;
    }
    runTest(argv[1], nodeIdstoRun);
}

void runTest(char *_testDir, ptr<set<node_id>> &_nodeIdstoRun) {



        uint64_t runTimeMs = 30000;

        signal(SIGPIPE, SIG_IGN);
        Node::nodeIDs = _nodeIdstoRun;

        fs_path dirPath(boost::filesystem::system_complete(fs_path(_testDir)));

        ConsensusEngine engine;

        engine.parseConfigsAndCreateAllNodes(dirPath);
        engine.slowStartBootStrapTest();

        usleep(1000 * runTimeMs);

        engine.exitGracefully();

        cerr << "Test completed successfully" << endl;


#ifdef GOOGLE_PROFILE
        HeapProfilerStop();
#endif


    }
