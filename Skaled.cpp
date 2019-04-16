




#include "SkaleConfig.h"

#include "node/ConsensusEngine.h"

#ifdef GOOGLE_PROFILE
#include <gperftools/heap-profiler.h>
#endif


int main(int argc, char **argv) {

#ifdef GOOGLE_PROFILE
    HeapProfilerStart("/tmp/consensusd.profile");
#endif

    signal(SIGPIPE, SIG_IGN);



    if (argc < 2) {
        printf("Usage: skaled nodes_dir node_id1 node_id2 \n");
        exit(1);
    }

    for (int i = 2; i < argc; i++) {

        uint64_t ui64;
        ui64 = static_cast<uint64_t >(stoll(argv[i]));

        Node::nodeIDs.insert(node_id(ui64));

        cerr << node_id(ui64) << endl;
    }

    fs_path dirPath(boost::filesystem::system_complete(fs_path(argv[1])));

    ConsensusEngine engine;

    engine.parseConfigsAndCreateAllNodes(dirPath);


    engine.slowStartBootStrapTest();

    sleep(100000);

    engine.exitGracefully();

    cerr << "Exited" << endl;


#ifdef GOOGLE_PROFILE
    HeapProfilerStop();
#endif




}
