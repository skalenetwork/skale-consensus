
#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/EngineInitException.h"
#include "EngineInitException.h"


EngineInitException::EngineInitException(const string &_message, const string &_className) :
        Exception("Engine init failed:" + _message, _className) {}

