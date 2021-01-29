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

    recursive_mutex m;

    static atomic<int64_t> totalSockets;

    file_descriptor descriptor = 0;

    string remoteIP;

    network_port remotePort;

    ptr<sockaddr_in> remoteAddr = nullptr;


    void closeSocket();


    int createTCPSocket();


public:


    file_descriptor getDescriptor() ;

    string& getConnectionIP() ;

    network_port getConnectionPort();


    static uint64_t getTotalSockets();


    virtual ~ClientSocket() {
        closeSocket();
        totalSockets--;
    }


    ClientSocket(Schain &_sChain, schain_index _destinationIndex, port_type portType);

};
