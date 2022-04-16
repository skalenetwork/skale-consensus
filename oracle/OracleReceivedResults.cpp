//
// Created by kladko on 10.01.22.
//

#include "SkaleCommon.h"
#include "Log.h"

#include "utils/Time.h"
#include "OracleErrors.h"
#include "OracleReceivedResults.h"

OracleReceivedResults::OracleReceivedResults(uint64_t _requredSigners, uint64_t _nodeCount) {
    requiredConfirmations = (_requredSigners - 1) / 2 + 1;
    nodeCount = _nodeCount;
    requestTime = Time::getCurrentTimeMs();
    signaturesBySchainIndex = make_shared<map<uint64_t, string>>();
    resultsByCount = make_shared<map<string, uint64_t>>();
}

uint64_t OracleReceivedResults::getRequestTime() const {
    return requestTime;
}

void OracleReceivedResults::insertIfDoesntExist(uint64_t _origin, string _unsignedResult, string _sig) {

    LOCK(m)

    if (signaturesBySchainIndex->count(_origin) > 0) {
        LOG(warn, "Duplicate OracleResponseMessage for result:" + _unsignedResult +
                  "} index:" + to_string(_origin));
        return;
    }

    signaturesBySchainIndex->insert({_origin, _sig});
    if (resultsByCount->count(_unsignedResult) == 0) {
        resultsByCount->insert({_unsignedResult, 1});
    } else {
        auto count = resultsByCount->at(_unsignedResult);
        resultsByCount->insert_or_assign(_unsignedResult, count + 1);
    }
}

uint64_t OracleReceivedResults::tryGettingResult(string &_result) {
    if (getRequestTime() + ORACLE_TIMEOUT_MS < Time::getCurrentTimeMs())
        return ORACLE_TIMEOUT;

    LOCK(m)

    vector<string> signatures;

    for (auto &&item: *resultsByCount) {
        if (item.second >= requiredConfirmations) {
            uint64_t  sigCount = 0;
            auto _unsignedResult = item.first;
            _unsignedResult.append("\"sigs\":[");
            for (uint64_t i = 1; i <= nodeCount; i++) {
                if (signaturesBySchainIndex->count(i) > 0 && sigCount < requiredConfirmations) {
                    _unsignedResult.append("\"");
                    auto signature = signaturesBySchainIndex->at(i);
                    _unsignedResult.append(signaturesBySchainIndex->at(i));
                    _unsignedResult.append("\"");
                    signatures.push_back(signature);
                    sigCount++;
                } else {
                    _unsignedResult.append("null");
                }

                if (i < nodeCount) {
                    _unsignedResult.append(",");
                } else {
                    _unsignedResult.append("],");
                }
            }

            _unsignedResult.append("\"abiEncodedSignedResult\":\"");

            auto abiEncodedSignedResult = "HAHA";

            _unsignedResult.append(abiEncodedSignedResult);

            _unsignedResult.append("\"}\"");

            _result = _unsignedResult;
            LOG(err, "ORACLE SUCCESS!");
            return ORACLE_SUCCESS;
        };
    }

    return ORACLE_RESULT_NOT_READY;

}

