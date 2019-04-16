//
// Created by stan on 23.11.18.
//
#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "CouldNotReadPartialDataHashesException.h"

CouldNotReadPartialDataHashesException::CouldNotReadPartialDataHashesException(const std::string &_message,
        const string& _className) :  Exception(
        _message, _className) {}
