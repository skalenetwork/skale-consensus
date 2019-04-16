#pragma once

#include "Exception.h"

class CouldNotReadPartialDataHashesException : public Exception {

public:
    CouldNotReadPartialDataHashesException(const std::string &_message,  const string& _className);
};

