/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skale-consensus.  If not, see <http://www.gnu.org/licenses/>.

    @file ZMQNetwork.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../SkaleCommon.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"

#include "../thirdparty/json.hpp"

#include "zmq.h"
#include "../abstracttcpserver/ConnectionStatus.h"
#include "TransportNetwork.h"

#include "../messages/NetworkMessage.h"
#include "../messages/NetworkMessageEnvelope.h"
#include "../node/Node.h"
#include "../node/NodeInfo.h"
#include "../exceptions/FatalError.h"
#include "../blockproposal/server/BlockProposalWorkerThreadPool.h"
#include "../blockproposal/pusher/BlockProposalClientAgent.h"
#include "../blockproposal/received/ReceivedBlockProposalsDatabase.h"
#include "../pendingqueue/PendingTransactionsAgent.h"
#include "../exceptions/NetworkProtocolException.h"
#include "../exceptions/IOException.h"
#include "../exceptions/ExitRequestedException.h"

#include "../chains/Schain.h"
#include "Sockets.h"
#include "ZMQNetwork.h"
#include "Buffer.h"

#include "ZMQServerSocket.h"

using namespace std;


bool ZMQNetwork::sendMessage(const ptr<NodeInfo> &_remoteNodeInfo, ptr<NetworkMessage> _msg) {

    auto buf = _msg->toBuffer();

    auto ip = _remoteNodeInfo->getBaseIP();

    auto port = _remoteNodeInfo->getPort();

    void *s = sChain->getNode()->getSockets()->consensusZMQSocket->getDestinationSocket(ip, port);

    auto len = buf->getCounter();

    ASSERT(len == CONSENSUS_MESSAGE_LEN);

#ifdef ZMQ_NONBLOCKING
    return interruptableSend(s, buf->getBuf()->data(), len, true);
#else
    return interruptableSend(s, buf->getBuf()->data(), len, false);
#endif


}


int ZMQNetwork::interruptableRecv(void *_socket, void *_buf, size_t _len, int _flags) {

    int rc = -1;

    do {

        rc = zmq_recv(_socket, _buf,
                      _len, _flags);
        if (this->getNode()->isExitRequested()) {
            LOG(debug, getThreadName() + " zmq debug: closing = " + to_string((uint64_t)_socket));
            int linger = 1;
            zmq_setsockopt(_socket, ZMQ_LINGER, &linger, sizeof(linger));
            zmq_close(_socket);
            throw ExitRequestedException();
        }

    } while (rc < 0 && errno == EAGAIN);

    if (rc < 0) {
        throw NetworkProtocolException("Zmq recv failed " + string(zmq_strerror(errno)), __CLASS_NAME__);
    }

    return rc;

}


bool ZMQNetwork::interruptableSend(void *_socket, void *_buf, size_t _len, bool _isNonBlocking) {


    usleep(1000 * sChain->getNode()->getSimulateNetworkWriteDelayMs());

    int rc = -1;


    int flags = 0;

    if (_isNonBlocking)
        flags = ZMQ_DONTWAIT;

    do {

        rc = zmq_send(_socket, _buf, _len, flags);

        if (this->getNode()->isExitRequested()) {
            LOG(debug, getThreadName() + "zmq debug: closing = " + to_string((uint64_t)_socket));
            zmq_close(_socket);
            throw ExitRequestedException();
        }

        if (_isNonBlocking && rc < 0 && errno == EAGAIN) {
            return false;
        }


    } while (rc < 0 && errno == EAGAIN);


    if (rc < 0) {
        throw NetworkProtocolException("Zmq send failed " + string(zmq_strerror(errno)), __CLASS_NAME__);
    }


    return true;
}


ptr<string> ZMQNetwork::readMessageFromNetwork(ptr<Buffer> buf) {

    auto s = sChain->getNode()->getSockets()->consensusZMQSocket->getReceiveSocket();

    auto rc = interruptableRecv(s, buf->getBuf()->data(), CONSENSUS_MESSAGE_LEN, 0);


    if (rc != CONSENSUS_MESSAGE_LEN) {
        throw NetworkProtocolException("Incorrect message length:" +
                                       to_string(rc), __CLASS_NAME__);
    }


#ifndef ZMQ_EXPERIMENTAL
    int zero = 0;
    interruptableSend(s, &zero, 1, 0);
#endif

    return make_shared<string>("");

}


ZMQNetwork::ZMQNetwork(Schain &_schain) : TransportNetwork(_schain) {}

void ZMQNetwork::confirmMessage(const ptr<NodeInfo>&
#ifndef ZMQ_EXPERIMENTAL
                                remoteNodeInfo
#endif
) {


#ifndef ZMQ_EXPERIMENTAL


    auto ip = remoteNodeInfo->getBaseIP();

    auto port = remoteNodeInfo->getPort();

    void *s = sChain->getNode()->getSockets()->consensusZMQSocket->getDestinationSocket(ip, port);

    char response[1];
    interruptableRecv(s, response, 1, 0);
#endif
}
