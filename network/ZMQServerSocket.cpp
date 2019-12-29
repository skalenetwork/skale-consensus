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

    @file ZMQServerSocket.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"

#include "zmq.h"
#include "exceptions/FatalError.h"
#include "ZMQServerSocket.h"

ZMQServerSocket::ZMQServerSocket(ptr<string> &_bindIP, uint16_t _basePort, port_type _portType) : ServerSocket(_bindIP,
                                                                                                               _basePort,
                                                                                                               _portType) {
    context = zmq_ctx_new();
}

void *ZMQServerSocket::getDestinationSocket(ptr<string> _ip, network_port _basePort) {

    lock_guard<mutex> lock(mainMutex);

    if (sendSockets.count(*_ip) > 0) {
        return sendSockets.at(*_ip);
    }

#ifdef ZMQ_EXPERIMENTAL
    void *requester = zmq_socket(context, ZMQ_CLIENT);
#else
    void *requester = zmq_socket (context, ZMQ_REQ);
#endif

    LOG(debug, getThreadName() + " zmq debug: requester = " +  to_string((uint64_t )requester));

    int timeout = ZMQ_TIMEOUT;
    int linger= 1000;

    zmq_setsockopt(requester, ZMQ_SNDTIMEO, &timeout, sizeof(int));
    zmq_setsockopt(requester, ZMQ_RCVTIMEO, &timeout, sizeof(int));
    zmq_setsockopt(requester, ZMQ_LINGER, &linger, sizeof(int));


    int result = zmq_connect(requester, ("tcp://" + *_ip + ":" + to_string(_basePort + BINARY_CONSENSUS)).c_str());
    LOG(debug, "Connected ZMQ socket" + to_string(result));
    sendSockets[*_ip] = requester;

    return requester;
}

void *ZMQServerSocket::getReceiveSocket()  {



    lock_guard<mutex> lock(mainMutex);


    if (!receiveSocket) {
#ifdef ZMQ_EXPERIMENTAL
        receiveSocket = zmq_socket(context, ZMQ_SERVER);
#else

        receiveSocket = zmq_socket (context, ZMQ_REP);


#endif

        LOG(debug, getThreadName() + " zmq debug: receiveSocket = " + to_string((uint64_t)receiveSocket));

        int timeout = ZMQ_TIMEOUT;
        int linger= 1000;

        zmq_setsockopt(receiveSocket, ZMQ_RCVTIMEO, &timeout, sizeof(int));
        zmq_setsockopt(receiveSocket, ZMQ_SNDTIMEO, &timeout, sizeof(int));
        zmq_setsockopt(receiveSocket, ZMQ_LINGER, &linger, sizeof(int));


        int rc = zmq_bind(receiveSocket, ("tcp://" + *bindIP + ":" + to_string(bindPort)).c_str());
        if (rc != 0) {
            BOOST_THROW_EXCEPTION(FatalError(string("Could not bind ZMQ server socket:") + zmq_strerror(errno)));
        }


        LOG(debug, "Successfull bound ZMQ socket");

    }
    return receiveSocket;
}


void ZMQServerSocket::closeReceive() {

    if(receiveSocket){
        receiveSocket = nullptr;
        zmq_close(receiveSocket);
    }
}


void ZMQServerSocket::closeSend() {
    for (auto &&item : sendSockets) {
        if(item.second){
            LOG(debug, getThreadName() + " zmq debug in closeSend(): closing " + to_string((uint64_t) item.second));
            zmq_close(item.second);
            item.second = nullptr;
        }// if
    }
}


void ZMQServerSocket::terminate() {
    closeSend();
    closeReceive();
    zmq_ctx_shutdown(context);
    zmq_ctx_term(context);
}


ZMQServerSocket::~ZMQServerSocket() {
}

