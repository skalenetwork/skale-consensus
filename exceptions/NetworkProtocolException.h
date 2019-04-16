#pragma  once

#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "Exception.h"

class NetworkProtocolException : public  Exception {
public:
    NetworkProtocolException(const std::string &_message, const string& _className);
};
