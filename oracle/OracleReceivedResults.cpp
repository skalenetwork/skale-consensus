//
// Created by kladko on 10.01.22.
//

#include "SkaleCommon.h"
#include "Log.h"

#include "utils/Time.h"
#include "OracleErrors.h"
#include "OracleReceivedResults.h"

OracleReceivedResults::OracleReceivedResults(uint64_t _requredSigners) : requiredSigners(_requredSigners)
               {
    requestTime = Time::getCurrentTimeMs();
    resultsBySchainIndex = make_shared<map<uint64_t, string>>();
    resultsByCount = make_shared<map<string, uint64_t>>();
}

uint64_t OracleReceivedResults::getRequestTime() const {
    return requestTime;
}

void OracleReceivedResults::insertIfDoesntExist(uint64_t _origin, string _result) {
    if (resultsBySchainIndex->count(_origin) > 0) {
        LOG(warn, "Duplicate OracleResponseMessage for result:" + _result +
                  " index:" + to_string(_origin));
        return;
    }

    resultsBySchainIndex->insert({_origin, _result});
    if (resultsByCount->count(_result) == 0) {
        resultsByCount->insert({_result, 1});
    } else {
        auto count = resultsByCount->at(_result);
        resultsByCount->insert({_result, count + 1});
    }
}

uint64_t OracleReceivedResults::tryGettingResult(string &_result) {
    if (getRequestTime() + ORACLE_QUEUE_TIMEOUT_MS < Time::getCurrentTimeMs())
        return ORACLE_TIMEOUT;

    for (auto &&item: *resultsByCount) {
        if (item.second >= requiredSigners) {
            _result = item.first;
            LOG(err, "ORACLE SUCCESS!");
            return ORACLE_SUCCESS;
        };
    }

    return ORACLE_RESULT_NOT_READY;

}

