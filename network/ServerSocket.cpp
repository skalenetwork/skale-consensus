#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../thirdparty/json.hpp"

#include "ServerSocket.h"
#include "Sockets.h"
#include "Utils.h"


ServerSocket::ServerSocket(ptr<string> &_bindIP, uint16_t _basePort, port_type _portType)
    : bindIP( _bindIP ) {

    bindPort = _basePort + _portType;

    ASSERT( Utils::isValidIpAddress( _bindIP ) );

    this->socketaddr = Sockets::createSocketAddress( bindIP, bindPort );

    LOG(debug, "Binding ip: " + *_bindIP + " " + to_string(bindPort) + " " +
               to_string(_basePort));

}

int ServerSocket::getDescriptor() {
    return descriptor;
}

ptr< std::string >& ServerSocket::getBindIP() {
    return bindIP;
}

uint32_t ServerSocket::getBindPort() {
    return bindPort;
}




ServerSocket::~ServerSocket() {
    closeSocket();
}


void ServerSocket::closeSocket() {
    close( descriptor );
}
