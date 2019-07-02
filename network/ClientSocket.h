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

    @file ClientSocket.h
    @author Stan Kladko
    @date 2018
*/

#pragma  once





class Node;
class NodeInfo;
class Schain;

class ClientSocket {


    file_descriptor descriptor;

    ptr<string> remoteIP;

    ptr<string> bindIP;

    network_port remotePort;

    ptr<sockaddr_in> remote_addr;

    ptr<sockaddr_in> bind_addr;

public:


    file_descriptor getDescriptor() ;

    ptr<string> &getConnectionIP() ;

    network_port getConnectionPort();

    ptr<sockaddr_in> getSocketaddr();


    virtual ~ClientSocket() {
        closeSocket();
    }

    int createTCPSocket();

    ClientSocket(Schain &_sChain, schain_index _destinationIndex, port_type portType);

    void closeSocket();

};
