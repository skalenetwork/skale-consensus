//
// Created by stan on 19.03.18.
//


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
