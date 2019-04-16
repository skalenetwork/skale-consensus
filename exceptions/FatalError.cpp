//
// Created by stan on 21.12.18.
//


#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "FatalError.h"

FatalError::FatalError(const std::string &_message, const string& _className) : Exception(_message, _className) {
    fatal = true;
}
