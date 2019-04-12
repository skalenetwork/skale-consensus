#pragma  once
#include "Exception.h"

class InvalidHashException : public Exception {
public:
    InvalidHashException(const std::string &_message, const string& _className);
};
