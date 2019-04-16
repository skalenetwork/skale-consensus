#pragma once

#include "../Agent.h"

class Connection;
class Schain;
class Buffer;
class TCPServerSocket;
class ServerSocket;
class Header;
class PartialHashesList;


class AbstractServerAgent : public Agent {

protected:


    const string name;

    ptr<thread> networkReadThread;

    ptr<ServerSocket> socket;


    mutex incomingTCPConnectionsMutex;

    condition_variable incomingTCPConnectionsCond;



    void send(ptr<Connection> _connectionEnvelope, ptr<Header> _header);



public:

    AbstractServerAgent(const string &_name, Schain &_schain, ptr<TCPServerSocket> _socket);

    ~AbstractServerAgent() override;


    queue<ptr<Connection>> incomingTCPConnections;




    void pushToQueueAndNotifyWorkers(ptr<Connection> connectionEnvelope);

    ptr<Connection> workerThreadWaitandPopConnection();

    static void workerThreadConnectionProcessingLoop(void* _params);


    void notifyAllConditionVariables() override;

// to be implemented by subclasses


    virtual void processNextAvailableConnection(ptr<Connection> _connection) = 0;


    virtual ptr<PartialHashesList> readPartialHashes(ptr<Connection> _connectionEnvelope_, nlohmann::json _jsonRequest);



    void acceptTCPConnectionsLoop();


    void createNetworkReadThread();
};






