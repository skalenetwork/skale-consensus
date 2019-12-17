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

    @file Connection.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"

#include "ServerConnection.h"

using namespace std;

atomic<uint64_t> ServerConnection::totalConnections = 0;

ServerConnection::ServerConnection(unsigned int descriptor, ptr<std::string> ip)  {

    incrementTotalConnections();

    this->descriptor = descriptor;
    this->ip = ip;

}

file_descriptor ServerConnection::getDescriptor()  {
    return descriptor;
}

ptr<string> ServerConnection::getIP() {
    return ip;
}

ServerConnection::~ServerConnection() {
    decrementTotalConnections();
    closeConnection();
}


void ServerConnection::closeConnection() {
    LOCK(m)
    if (descriptor != 0)
        close((int)descriptor);
    descriptor = 0;
}

void ServerConnection::incrementTotalConnections() {
//    LOG(trace, "+Connections: " + to_string(totalConnections));
    CHECK_STATE(totalConnections < 1000000);
    totalConnections++;

}


void ServerConnection::decrementTotalConnections() {
    CHECK_STATE(totalConnections < 1000000);
    totalConnections--;
//    LOG(trace, "-Connections: " + to_string(totalConnections));

}

uint64_t ServerConnection::getTotalConnections() {
    return totalConnections;
};
