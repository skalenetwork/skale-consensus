#pragma once

#include "IOException.h"

class ConnectionRefusedException : public IOException {
public:

    ConnectionRefusedException(const string &_what, int _errno,  const string& _className);

};
