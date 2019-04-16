#pragma  once





class Node;
class NodeInfo;
class Schain;

class ClientSocket {


    file_descriptor descriptor;

    ptr<string> remoteIP;

    ptr<string> bindIP;

    network_port remotePort;

    ptr<sockaddr_in> remote_addr;

    ptr<sockaddr_in> bind_addr;

public:


    file_descriptor getDescriptor() ;

    ptr<string> &getConnectionIP() ;

    network_port getConnectionPort();

    ptr<sockaddr_in> getSocketaddr();


    virtual ~ClientSocket() {
        closeSocket();
    }

    int createTCPSocket();

    ClientSocket(Schain &_sChain, schain_index _destinationIndex, port_type portType);

    void closeSocket();

};
