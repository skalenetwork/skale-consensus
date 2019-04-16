
#include "../SkaleConfig.h"
#include "../Log.h"
#include "InvalidArgumentException.h"

InvalidArgumentException::InvalidArgumentException(const std::string &_message, const string& _className) :
                      Exception(_message, _className) {
    fatal = false;
}
