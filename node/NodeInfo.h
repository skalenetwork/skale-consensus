#pragma  once

class NodeInfo {

    node_id nodeID;

    ptr<string> ip;

    network_port port;

    schain_id  schainID;

    schain_index schainIndex;

    ptr<sockaddr_in> socketaddr;


public:


    node_id getNodeID() const;

    schain_index getSchainIndex() const;

    network_port getPort() const;


public:

    schain_id getSchainID() const;

    NodeInfo(node_id nodeID, ptr<string> &ip, network_port port, schain_id schainID, schain_index schainIndex);


    ptr<sockaddr_in> getSocketaddr();

    ptr<string> getBaseIP();
};

