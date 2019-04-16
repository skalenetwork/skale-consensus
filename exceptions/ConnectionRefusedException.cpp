//
// Created by skale on 12/31/18.
//

#include "ConnectionRefusedException.h"

ConnectionRefusedException::ConnectionRefusedException(const string &_what, int _errno,  const string& _className) :
                                    IOException(_what, _errno,  _className) {};
