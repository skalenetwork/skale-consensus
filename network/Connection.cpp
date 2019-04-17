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

    @file Connection.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"

#include "Connection.h"

using namespace std;

uint64_t Connection::totalConnections = 0;

Connection::Connection(unsigned int descriptor, ptr<std::string> ip)  {

    incrementTotalConnections();

    this->descriptor = descriptor;
    this->ip = ip;

}

file_descriptor Connection::getDescriptor()  {
    return descriptor;
}

ptr<string> Connection::getIP() {
    return ip;
}

Connection::~Connection() {
    decrementTotalConnections();
    close((int)descriptor);
}

void Connection::incrementTotalConnections() {
//    LOG(trace, "+Connections: " + to_string(totalConnections));
    totalConnections++;

}


void Connection::decrementTotalConnections() {
    totalConnections--;
//    LOG(trace, "-Connections: " + to_string(totalConnections));

}

uint64_t Connection::getTotalConnections() {
    return totalConnections;
};
