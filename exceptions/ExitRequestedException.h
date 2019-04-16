#pragma  once
#include "Exception.h"

class ExitRequestedException : public Exception {
public:
    ExitRequestedException();
};
