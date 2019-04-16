#pragma once


class ProtocolInstance;
class Node;

class InstanceGarbageCollectorAgent {
    // Node* node; // l_sergiy: clang did detected this as unused

    static thread* garbageThread;
    static bool exitRequested;

public:
    static queue< ProtocolInstance* > garbageQueue;

    static mutex garbageMutex;

    InstanceGarbageCollectorAgent( Node& node_ );
    ~InstanceGarbageCollectorAgent();

    static void scheduleForDeletion( ProtocolInstance& pi_ );

    static void cleanQueue();

    static void garbageLoop();

    static void exitGracefully();
};
