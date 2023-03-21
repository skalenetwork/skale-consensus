//
// Created by kladko on 10.01.22.
//

#include "SkaleCommon.h"
#include "Log.h"

#include "utils/Time.h"
#include "network/Utils.h"
#include "OracleErrors.h"
#include "OracleRequestSpec.h"
#include "OracleResult.h"
#include "OracleReceivedResults.h"


OracleReceivedResults::OracleReceivedResults(ptr<OracleRequestSpec> _requestSpec, uint64_t _requredSigners,
                                             uint64_t _nodeCount, bool _isSgx) {
    requestSpec = _requestSpec;
    requiredConfirmations = (_requredSigners - 1) / 2 + 1;
    nodeCount = _nodeCount;
    requestTime = Time::getCurrentTimeMs();
    signaturesBySchainIndex = make_shared<map<uint64_t, string>>();
    resultsByCount = make_shared<map<string, uint64_t>>();
    isSgx = _isSgx;
}

uint64_t OracleReceivedResults::getRequestTime() const {
    return requestTime;
}

void OracleReceivedResults::insertIfDoesntExist(uint64_t _origin, ptr<OracleResult> _oracleResult) {

    try {


        CHECK_STATE(_oracleResult);


        auto unsignedResult = _oracleResult->getUnsignedOracleResult();
        auto sig = _oracleResult->getSig();

        LOCK(m)

        if (signaturesBySchainIndex->count(_origin) > 0) {
            LOG(warn, "Duplicate OracleResponseMessage for result:" + unsignedResult +
                      "} index:" + to_string(_origin));
            return;
        }

        signaturesBySchainIndex->insert({_origin, sig});

        if (resultsByCount->count(unsignedResult) == 0) {
            resultsByCount->insert({unsignedResult, 1});
        } else {
            auto count = resultsByCount->at(unsignedResult);
            resultsByCount->insert_or_assign(unsignedResult, count + 1);
        }

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}

string OracleReceivedResults::compileCompleteResultJson(string &_unsignedResult) {

    try {
        uint64_t sigCount = 0;
        auto completeResult = _unsignedResult;
        completeResult.append("\"sigs\":[");
        for (uint64_t i = 1; i <= nodeCount; i++) {
            if (signaturesBySchainIndex->count(i) > 0 && sigCount < requiredConfirmations) {
                completeResult.append("\"");
                completeResult.append(signaturesBySchainIndex->at(i));
                completeResult.append("\"");
                sigCount++;
            } else {
                completeResult.append("null");
            }

            if (i < nodeCount) {
                completeResult.append(",");
            } else {
                completeResult.append("]}");
            }
        }
        return completeResult;
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


vector<uint8_t>  OracleReceivedResults::ecdsaSigStringToByteArray(string& _sig ) {


    if (this->isSgx) {

        boost::char_separator< char > sep( ":");
        boost::tokenizer tok {_sig, sep};
        vector<string> tokens;

        for ( const auto& it : tok) {
            tokens.push_back((it));
        }

        CHECK_STATE(tokens.size() == 3)


        string tmp = "0" + tokens[0];

        for (uint64_t i = 0; i < 64 - tokens[1].size(); i++) {
            tmp.append("0");
        }

        tmp.append(tokens[1]);

        for (uint64_t i = 0; i < 64 - tokens[2].size(); i++) {
            tmp.append("0");
        }

        tmp.append(tokens[2]);

        CHECK_STATE(tmp.size() == 130);

        vector<uint8_t> result(tmp.size() / 2);
        Utils::cArrayFromHex(tmp, result.data(), result.size());
        return result;
    } else {
        CHECK_STATE(_sig.size() % 2 == 0);
        vector<uint8_t> result(_sig.size() / 2);
        Utils::cArrayFromHex(_sig, result.data(), result.size());
        return result;
    }
}

string OracleReceivedResults::compileCompleteResultRlp(string &_unsignedResult) {

    RLPOutputStream resultWithSignaturesStream(1 + nodeCount);


    try {
        uint64_t sigCount = 0;
        resultWithSignaturesStream.append(_unsignedResult);
        for (uint64_t i = 1; i <= nodeCount; i++) {
            if (signaturesBySchainIndex->count(i) > 0 && sigCount < requiredConfirmations) {
                resultWithSignaturesStream.append(ecdsaSigStringToByteArray(signaturesBySchainIndex->at(i)));
                sigCount++;
            } else {
                resultWithSignaturesStream.append("");
            }
        }

        auto rawResult = resultWithSignaturesStream.out();

        return Utils::carray2Hex(rawResult.data(), rawResult.size());

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


uint64_t OracleReceivedResults::tryGettingResult(string &_result) {

    try {
        if (getRequestTime() + ORACLE_TIMEOUT_MS < Time::getCurrentTimeMs())
            return ORACLE_TIMEOUT;

        LOCK(m)

        for (auto &&item: *resultsByCount) {
            if (item.second >= requiredConfirmations) {
                string unsignedResult = item.first;
                auto encoding = requestSpec->getEncoding();
                if (encoding == ORACLE_ENCODING_JSON) {
                    _result = compileCompleteResultJson(unsignedResult);
//                } else if (encoding == ORACLE_ENCODING_ABI|| encoding.empty()){
//                    // JSON by default
//                    _result = compileCompleteResultAbi(unsignedResult);
                } else {
                    // should never get to this line
                    CHECK_STATE(false);
                }
                return ORACLE_SUCCESS;
            };
        }

        return ORACLE_RESULT_NOT_READY;

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}

const ptr<OracleRequestSpec> &OracleReceivedResults::getRequestSpec() const {
    return requestSpec;
}

