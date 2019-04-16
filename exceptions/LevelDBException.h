#pragma  once
#include "Exception.h"

class LevelDBException : public Exception {
public:
    LevelDBException(const std::string &_message, const string& _className);






};
