#pragma once

#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "Exception.h"

class InvalidSchainIndexException : public Exception {
public:
    InvalidSchainIndexException(const string &_message, const string _className) : Exception(_message, _className) {}

};
