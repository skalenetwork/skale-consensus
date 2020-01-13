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

    @file ZMQNetwork.h
    @author Stan Kladko
    @date 2018
*/

#pragma  once

#include "TransportNetwork.h"
#include "Buffer.h"

class Node;
class NetworkMessage;
class NodeInfo;

class ClientSocket;
class NetworkMessageEnvelope;
class ServerConnection;

class Schain;


class TransactionList;


class ZMQNetwork : public TransportNetwork {


public:


    uint64_t interruptableRecv(void *_socket, void *_buf, size_t _len, int _flags);

    bool interruptableSend(void *_socket, void *_buf, size_t _len, bool _isNonBlocking = false);

    uint64_t readMessageFromNetwork(ptr<Buffer> buf);

    ZMQNetwork(Schain &_schain);

    bool sendMessage(const ptr<NodeInfo> &_remoteNodeInfo, ptr<NetworkMessage> _msg);

    virtual void confirmMessage(const ptr<NodeInfo> &remoteNodeInfo);
};

