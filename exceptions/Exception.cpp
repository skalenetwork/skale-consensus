//
// Created by stan on 21.12.18.
//

#include "../SkaleConfig.h"
#include "../Log.h"
#include "Exception.h"

void Exception::log_exception(const std::exception& e, int level)
{
    string prefix;

    if (level == 0) {
        prefix = "!Exception:";
    } else {
        prefix = "!Caused by:";
    }
    LOG(err, string(level, ' ') + prefix + e.what());
    try {
        std::rethrow_if_nested(e);
    } catch(const std::exception& e) {
        log_exception(e, level+1);
    } catch(...) {}
}