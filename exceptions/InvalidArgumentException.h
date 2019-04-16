#pragma  once
#include "Exception.h"

class InvalidArgumentException : public Exception {
public:
    InvalidArgumentException(const std::string &_message, const string& _className);
};
