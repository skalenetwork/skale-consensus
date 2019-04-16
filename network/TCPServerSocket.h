#pragma once




#include "ServerSocket.h"


class TCPServerSocket : public ServerSocket{


    int createAndBindTCPSocket();

public:

    TCPServerSocket(ptr<string> &_bindIP, uint16_t _basePort, port_type  _portType);

    void touch();



    virtual ~TCPServerSocket();

};
