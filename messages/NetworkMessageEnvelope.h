#pragma  once

class NodeInfo;

class NetworkMessage;
class Node;
class ProtocolKey;

#include "MessageEnvelope.h"


class NetworkMessageEnvelope : public MessageEnvelope {

public:
    NetworkMessageEnvelope(const ptr <NetworkMessage> &message,
                           const ptr <NodeInfo> &realSender);

};

