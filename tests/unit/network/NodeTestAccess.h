#pragma once

#include "node/Node.h"

class NodeTestAccess {
public:
    static const SocketPortDeltas& getSocketPortDeltas( const Node& _node ) {
        return _node.socketPortDeltas;
    }
};
