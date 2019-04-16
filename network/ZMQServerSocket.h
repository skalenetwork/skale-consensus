#pragma once


#include "ServerSocket.h"


static const int ZMQ_TIMEOUT = 1000;

class ZMQServerSocket : public ServerSocket {

    mutex mainMutex;

    void *context;

    map<string, void *> sendSockets;

    void *receiveSocket = nullptr;

public:


    ZMQServerSocket(ptr<string> &_bindIP, uint16_t _basePort, port_type _portType);


    void *getReceiveSocket();

    void *getDestinationSocket(ptr<string> _ip, network_port _basePort);


    void closeReceive();

    void closeSend();

    void terminate();

    virtual ~ZMQServerSocket();


};

