#pragma once




#include <arpa/inet.h>
#include <mutex>

#include <memory>
#include "../node/Node.h"


class ServerSocker;
class TCPServerSocket;
class ZMQServerSocket;

class Sockets {

    Node& node;
public:
    Node &getNode() const;

public:
    Sockets(Node &node);

public:



    ptr<ZMQServerSocket> consensusZMQSocket = nullptr;

    const ptr<ZMQServerSocket> &getConsensusZMQSocket() const;


    ptr<TCPServerSocket> blockProposalSocket;

    ptr<TCPServerSocket> catchupSocket;


    static ptr<sockaddr_in> createSocketAddress(ptr<string> &ip, uint16_t port);

    void initSockets(ptr<string> &bindIP, uint16_t basePort);

};



