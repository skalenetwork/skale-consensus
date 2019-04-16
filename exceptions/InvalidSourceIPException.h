#pragma once

#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "Exception.h"

class InvalidSourceIPException : public Exception {
public:
    InvalidSourceIPException(const string &_message, const string _className = "") : Exception(_message, _className) {}

};


