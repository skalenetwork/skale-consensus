#pragma once




#include "../Agent.h"

class Schain;

class NetworkMessageEnvelope;
class NodeInfo;
class NetworkMessage;
class Buffer;
class Node;
class Schain;

enum TransportType {ZMQ};

class TransportNetwork : public Agent  {

    // used to test catchup

    recursive_mutex delayedSendsLock;

    vector<std::list<pair<ptr<NetworkMessage>,ptr<NodeInfo>>>> delayedSends;


    recursive_mutex deferredMutex;

    uint32_t packetLoss = 0;
public:
    uint32_t getPacketLoss() const;

    uint64_t getCatchupBlock() const;

private:
    uint64_t   catchupBlocks = 0;


protected:

    static TransportType transport;


    explicit TransportNetwork(Schain& _sChain);
    /**
     * Mutex that controls access to inbox
     */
    recursive_mutex deferredMessageMutex;

    map<block_id, ptr<vector<ptr<NetworkMessageEnvelope>>>> deferredMessageQueue;

    void addToDeferredMessageQueue(ptr<NetworkMessageEnvelope> _me);

    ptr<vector<ptr<NetworkMessageEnvelope>>> pullMessagesForBlockID(block_id _blockID);


    virtual bool sendMessage(const ptr<NodeInfo> &remoteNodeInfo, ptr<NetworkMessage> _msg) = 0;


    virtual void confirmMessage(const ptr<NodeInfo> &remoteNodeInfo);



    ptr<thread> networkReadThread;

    ptr<thread> deferredMessageThread;


public:


    void startThreads();

    void deferredMessagesLoop();

    void networkReadLoop();

    void waitUntilExit();



    ptr<string> ipToString(uint32_t _ip);

    void broadcastMessage(Schain& _schain, ptr<NetworkMessage> _m);

    ptr<NetworkMessageEnvelope> receiveMessage();

    virtual ptr<string> readMessageFromNetwork(ptr<Buffer> buf) = 0;

    static bool validateIpAddress(ptr<string> &_ip);

    static void setTransport(TransportType transport);

    static TransportType getTransport();

    void setPacketLoss(uint32_t packetLoss);

    void setCatchupBlocks(uint64_t _catchupBlocks);

    void postOrDefer(const ptr<NetworkMessageEnvelope> &m, const block_id &currentBlockID);
};
