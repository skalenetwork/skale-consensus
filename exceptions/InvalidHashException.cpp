
#include "../SkaleConfig.h"
#include "../Log.h"
#include "InvalidHashException.h"

InvalidHashException::InvalidHashException(const std::string &_message, const string& _className) :
                      Exception(_message, _className) {
    fatal = false;
}
