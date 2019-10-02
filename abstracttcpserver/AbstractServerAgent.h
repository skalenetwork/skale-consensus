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

    @file AbstractServerAgent.h
    @author Stan Kladko
    @date 2018
*/

#pragma once

#include "../Agent.h"

class ServerConnection;
class Schain;
class Buffer;
class TCPServerSocket;
class ServerSocket;
class Header;
class PartialHashesList;


class AbstractServerAgent : public Agent {

protected:


    const string name;

    thread* networkReadThread;

    ptr<ServerSocket> socket;


    mutex incomingTCPConnectionsMutex;

    condition_variable incomingTCPConnectionsCond;



    void send(ptr<ServerConnection> _connectionEnvelope, ptr<Header> _header);



public:

    AbstractServerAgent(const string &_name, Schain &_schain, ptr<TCPServerSocket> _socket);

    ~AbstractServerAgent() override;


    queue<ptr<ServerConnection>> incomingTCPConnections;




    void pushToQueueAndNotifyWorkers(ptr<ServerConnection> connectionEnvelope);

    ptr<ServerConnection> workerThreadWaitandPopConnection();

    static void workerThreadConnectionProcessingLoop(void* _params);


    void notifyAllConditionVariables() override;

// to be implemented by subclasses


    virtual void processNextAvailableConnection(ptr<ServerConnection> _connection) = 0;


    virtual ptr<PartialHashesList> readPartialHashes(ptr<ServerConnection> _connectionEnvelope_, nlohmann::json _jsonRequest);



    void acceptTCPConnectionsLoop();


    void createNetworkReadThread();
};






