#pragma once
#include "../Log.h"
#include "Exception.h"

class ParsingException : public Exception  {
public:
    ParsingException(const std::string &_message,  const string& _className);

};
