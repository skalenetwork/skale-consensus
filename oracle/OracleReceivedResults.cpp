//
// Created by kladko on 10.01.22.
//

#include "SkaleCommon.h"
#include "Log.h"

#include "utils/Time.h"
#include "OracleReceivedResults.h"

OracleReceivedResults::OracleReceivedResults() {
    requestTime = Time::getCurrentTimeMs();
    resultsBySchainIndex = make_shared<map<uint64_t, string>>();
    resultsByCount = make_shared<map<string, uint64_t>>();
}

uint64_t OracleReceivedResults::getRequestTime() const {
    return requestTime;
}
