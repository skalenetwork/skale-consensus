
#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/LevelDBException.h"
#include "LevelDBException.h"


LevelDBException::LevelDBException(const string &_message, const string &_className) :
        Exception("LevelDB:" + _message, _className) {}

