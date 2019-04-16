#pragma  once
#include "Exception.h"

class FatalError : public Exception {
public:
    FatalError(const std::string &_message, const string& _className = "");

};
