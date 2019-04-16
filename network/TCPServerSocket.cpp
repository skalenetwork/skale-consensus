#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"

#include "../exceptions/FatalError.h"
#include <boost/asio.hpp>
#include "TCPServerSocket.h"

int TCPServerSocket::createAndBindTCPSocket() {
    LOG(debug, "Creating TCP listen socket");
    int s;

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {

        BOOST_THROW_EXCEPTION(FatalError("Could not create read socket"));

    }


    int iSetOption = 1;


    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &iSetOption, sizeof(iSetOption));


    if (::bind(s, (struct sockaddr *) socketaddr.get(), sizeof(sockaddr_in)) < 0) {

        BOOST_THROW_EXCEPTION(FatalError("Could not bind the TCP socket: error " + to_string(errno)));

    }

    // Init the connection
    listen(s, SOCKET_BACKLOG);

    LOG(debug, "Successfully created TCP listen socket");

    return s;
}


TCPServerSocket::TCPServerSocket(ptr<string> &_bindIP, uint16_t _basePort, port_type _portType) : ServerSocket(_bindIP,
                                                                                                               _basePort,
                                                                                                               _portType) {
    descriptor = createAndBindTCPSocket();
    ASSERT(descriptor > 0);
}


TCPServerSocket::~TCPServerSocket() {
    close(descriptor);
}

void TCPServerSocket::touch() {
    using namespace boost::asio;
    ip::tcp::endpoint ep( ip::address::from_string(*bindIP), bindPort);
    io_service service;
    ip::tcp::socket sock(service);
    sock.connect(ep);
}


