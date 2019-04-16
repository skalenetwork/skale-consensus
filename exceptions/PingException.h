//
// Created by kladko on 3/19/19.
//

#ifndef CONSENSUS_PINGEXCEPTION_H
#define CONSENSUS_PINGEXCEPTION_H


#include "NetworkProtocolException.h"

class PingException : public NetworkProtocolException {
public:
    PingException(const string &_message, const string &_className);

};


#endif //CONSENSUS_PINGEXCEPTION_H
