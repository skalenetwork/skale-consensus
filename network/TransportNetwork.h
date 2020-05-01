/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with skale-consensus.  If not, see <https://www.gnu.org/licenses/>.

    @file TransportNetwork.h
    @author Stan Kladko
    @date 2018
*/

#pragma once




#include "Agent.h"

class Schain;

class NetworkMessageEnvelope;
class NodeInfo;
class NetworkMessage;
class Buffer;
class Node;
class Schain;

enum TransportType {ZMQ};

class TransportNetwork : public Agent  {

    vector<recursive_mutex> delayedSendsLocks;

    vector<list<pair<ptr<NetworkMessage>,ptr<NodeInfo>>>> delayedSends;


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

    virtual void addToDeferredMessageQueue(ptr<NetworkMessageEnvelope> _me);

    ptr<vector<ptr<NetworkMessageEnvelope> > > pullMessagesForCurrentBlockID();

    virtual bool sendMessage(const ptr<NodeInfo> &remoteNodeInfo, ptr<NetworkMessage> _msg) = 0;


    ptr<thread> networkReadThread;

    ptr<thread> deferredMessageThread;


public:


    void startThreads();

    void deferredMessagesLoop();

    void networkReadLoop();

    void waitUntilExit();



    static ptr<string> ipToString(uint32_t _ip);

    void broadcastMessage(ptr<NetworkMessage> _m);

    ptr<NetworkMessageEnvelope> receiveMessage();

    virtual uint64_t readMessageFromNetwork(ptr<Buffer> buf) = 0;

    static bool validateIpAddress(ptr<string> &_ip);

    static void setTransport(TransportType transport);

    static TransportType getTransport();

    void setPacketLoss(uint32_t packetLoss);

    void setCatchupBlocks(uint64_t _catchupBlocks);

    void postDeferOrDrop(const ptr<NetworkMessageEnvelope> &m);

    ~TransportNetwork();

    void addToDelayedSends(ptr<NetworkMessage> _m, ptr<NodeInfo> dstNodeInfo);

    void trySendingDelayedSends();
};
