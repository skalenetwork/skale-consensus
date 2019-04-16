//
// Created by stan on 23.11.18.
//


#include "../SkaleConfig.h"
#include "../Log.h"
#include "NetworkProtocolException.h"

NetworkProtocolException::NetworkProtocolException(const std::string &_message,
        const string & _className) : Exception(_message, _className) {}
