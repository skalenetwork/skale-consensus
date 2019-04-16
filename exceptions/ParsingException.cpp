//
// Created by stan on 21.12.18.
//



#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "ParsingException.h"

ParsingException::ParsingException(const std::string &_message,  const string& _className)
                                                                                    : Exception(_message, _className) {


}
