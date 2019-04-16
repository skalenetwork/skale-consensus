#pragma once


class ServerSocket {
    // Node& node; // l_sergiy: clang did detected this as unused


protected:


    int descriptor = 0;

    ptr< string > bindIP;

    uint32_t bindPort;

    ptr< sockaddr_in > socketaddr;

public:

    int getDescriptor();

    ptr< string >& getBindIP();

    uint32_t getBindPort();

    ServerSocket(ptr<string> &_bindIP, uint16_t _basePort, port_type  _portType);

    virtual ~ServerSocket();

    void closeSocket();
};
