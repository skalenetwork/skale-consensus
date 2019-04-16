#pragma  once

#include "TransportNetwork.h"
#include "Buffer.h"

class Node;
class NetworkMessage;
class NodeInfo;

class ClientSocket;
class NetworkMessageEnvelope;
class Connection;

class Schain;


class TransactionList;


class ZMQNetwork : public TransportNetwork {


public:


    int interruptableRecv(void *_socket, void *_buf, size_t _len, int _flags);

    bool interruptableSend(void *_socket, void *_buf, size_t _len, bool _isNonBlocking = false);

    ptr<string> readMessageFromNetwork(ptr<Buffer> buf);

    ZMQNetwork(Schain &_schain);

    bool sendMessage(const ptr<NodeInfo> &_remoteNodeInfo, ptr<NetworkMessage> _msg);

    virtual void confirmMessage(const ptr<NodeInfo> &remoteNodeInfo);
};

