#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "IOException.h"

IOException::IOException(string _what, int _errno, const string& _className) : NetworkProtocolException(_what + ":" + strerror(_errno), _className) {
}
