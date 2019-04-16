//
// Created by stan on 17.03.18.
//


#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../thirdparty/json.hpp"

#include "../network/Sockets.h"
#include "NodeInfo.h"


using namespace std;




node_id NodeInfo::getNodeID() const {
    return nodeID;
}




schain_index NodeInfo::getSchainIndex() const {
    return schainIndex;
}

network_port NodeInfo::getPort() const {
    return port;
}




ptr<sockaddr_in> NodeInfo::getSocketaddr() {
    return socketaddr;
}

ptr<string> NodeInfo::getBaseIP() {
    return ip;
}



schain_id NodeInfo::getSchainID() const {
    return schainID;
}


NodeInfo::NodeInfo(node_id nodeID, ptr<string> &ip, network_port port, schain_id schainID, schain_index schainIndex) :
                                                                                                           nodeID(nodeID),
                                                                                                           ip(ip),
                                                                                                           port(port),
                                                                                                           schainID(
                                                                                                                   schainID),
                                                                                                           schainIndex(
                                                                                                                   schainIndex) {
        this->socketaddr = Sockets::createSocketAddress(ip, (uint16_t)port);
}
