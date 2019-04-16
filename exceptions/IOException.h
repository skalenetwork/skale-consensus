#pragma  once

#include "NetworkProtocolException.h"

class IOException : public  NetworkProtocolException {

    int errno;

public:

    IOException(string _what, int _errno, const string& _className);
};

