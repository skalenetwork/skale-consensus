//
// Created by kladko on 25.09.19.
//

#include "../SkaleCommon.h"
#include "../Log.h"
#include "Time.h"


using namespace std::chrono;

uint64_t Time::getCurrentTimeSec() {
    uint64_t result = getCurrentTimeMs() / 1000;
    ASSERT(result < (uint64_t) MODERN_TIME + 1000000000);
    return result;
}


uint64_t Time::getCurrentTimeMs() {
    uint64_t result = chrono::duration_cast<chrono::milliseconds>(
            chrono::system_clock::now().time_since_epoch()).count();
    return result;
}


