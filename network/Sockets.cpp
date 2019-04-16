//
// Created by stan on 28.03.18.
//


#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../thirdparty/json.hpp"
#include "TransportNetwork.h"
#include "Sockets.h"
#include "TCPServerSocket.h"
#include "ZMQServerSocket.h"


using namespace std;

void Sockets::initSockets(ptr<string> &bindIP, uint16_t basePort) {



    LOG(debug, "Initing network processing\n");

    consensusZMQSocket = make_shared<ZMQServerSocket>(bindIP, basePort, BINARY_CONSENSUS);


    blockProposalSocket = make_shared<TCPServerSocket>(bindIP, basePort, PROPOSAL);
    catchupSocket = make_shared<TCPServerSocket>(bindIP, basePort, CATCHUP);
}





ptr<sockaddr_in> Sockets::createSocketAddress(ptr<string>& ip, uint16_t port) {


    ASSERT(TransportNetwork::validateIpAddress(ip) > 0);


    sockaddr_in *a = new sockaddr_in();//(sockaddr_in *) malloc(sizeof(sockaddr_in));

    memset(a, 0, sizeof(struct sockaddr_in));

    uint32_t ipbin = inet_addr(ip->c_str());

    a->sin_family = AF_INET;
    a->sin_port = htons(port);
    a->sin_addr.s_addr = ipbin;


    return ptr<sockaddr_in>(a);
}

Sockets::Sockets(Node &node) : node(node) {}

Node &Sockets::getNode() const {
    return node;
}

const ptr<ZMQServerSocket> &Sockets::getConsensusZMQSocket() const {
    return consensusZMQSocket;
}

