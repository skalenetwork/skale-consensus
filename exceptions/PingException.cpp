//
// Created by kladko on 3/19/19.
//

#include "PingException.h"

PingException::PingException(const string &_message, const string &_className) : NetworkProtocolException(_message,
                                                                                                          _className) {}
