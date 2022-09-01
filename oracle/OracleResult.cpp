//
// Created by kladko on 14.01.22.
//

#include "thirdparty/rapidjson/document.h"
#include "thirdparty/json.hpp"
#include "thirdparty/rapidjson/prettywriter.h" // for stringify JSON

#include "SkaleCommon.h"
#include "Log.h"
#include "rlp/RLP.h"
#include "network/Utils.h"
#include "crypto/CryptoManager.h"
#include "OracleRequestSpec.h"
#include "OracleResult.h"

void OracleResult::parseResultAsJson() {
    rapidjson::Document d;

    d.Parse(oracleResult.data());

    CHECK_STATE2(!d.HasParseError(), "Unparsable Oracle result:" + oracleResult);

    CHECK_STATE2(d.HasMember("cid"), "No chainid in Oracle  result:" + oracleResult);

    CHECK_STATE2(d["cid"].IsUint64(), "ChainId in Oracle result is not uint64_t" + oracleResult);

    chainId = d["cid"].GetUint64();

    CHECK_STATE2(d.HasMember("uri"), "No URI in Oracle  result:" + oracleResult);

    CHECK_STATE2(d["uri"].IsString(), "Uri in Oracle result is not string:" + oracleResult);

    uri = d["uri"].GetString();

    CHECK_STATE(uri.size() > 5);

    CHECK_STATE2(d.HasMember("jsps"), "No json pointer in Oracle result:" + oracleResult);

    CHECK_STATE2(d["jsps"].IsArray(), "Jsps in Oracle spec is not array:" + oracleResult);

    CHECK_STATE2(d.HasMember("time"), "No time pointer in Oracle result:" + oracleResult);

    CHECK_STATE2(d["time"].IsUint64(), "time in Oracle result is not uint64:" + oracleResult)

    requestTime = d["time"].GetUint64();

    CHECK_STATE(requestTime > 0);

    CHECK_STATE2(d.HasMember("sig"), "No sig in Oracle result:" + oracleResult);

    CHECK_STATE2(d["sig"].IsString(), "sig in Oracle result is not string:" + oracleResult)

    sig = d["sig"].GetString();

    auto array = d["jsps"].GetArray();

    for (auto &&item: array) {
        CHECK_STATE2(item.IsString(), "Jsp array item is not string:" + oracleResult);
        string jsp  = item.GetString();
        CHECK_STATE2(!jsp.empty() && jsp.front() == '/', "Invalid JSP pointer:" + jsp);
        jsps.push_back(jsp);
    }

    if (d.HasMember("trims")) {
        auto trimArray = d["trims"].GetArray();
        for (auto &&item: trimArray) {
            CHECK_STATE2(item.IsUint64(), "Trims array item is not uint64 :" + oracleResult);
            trims.push_back(item.GetUint64());
        }

        CHECK_STATE2(jsps.size() == trims.size(), "jsps array size not equal trims array size");
    } else {
        for (uint64_t i = 0; i < jsps.size(); i++) {
            trims.push_back(0);
        }
    }


    if (d.HasMember("err")) {
        CHECK_STATE2(d["err"].IsUint64(), "Error is not uint64_t");
        error = d["err"].GetUint64();
        return;
    }

    auto resultsArray = d["rslts"].GetArray();

    results = make_shared<vector<ptr<string>>>();
    for (auto &&item: resultsArray) {
        if (item.IsString()) {
            results->push_back(make_shared<string>(item.GetString()));
        } else if (item.IsNull()) {
            results->push_back(nullptr);
        } else {
            CHECK_STATE2(false, "Unknown item in results:" + oracleResult)
        }
    }

    if (d.HasMember("post")) {
        CHECK_STATE2(d["post"].IsString(), "Post in Oracle result is not string:" + oracleResult);
        post = d["post"].GetString();
    }
}


void OracleResult::parseResultAsRlp() {
    rapidjson::Document d;

    d.Parse(oracleResult.data());

    CHECK_STATE2(!d.HasParseError(), "Unparsable Oracle result:" + oracleResult);

    CHECK_STATE2(d.HasMember("cid"), "No chainid in Oracle  result:" + oracleResult);

    CHECK_STATE2(d["cid"].IsUint64(), "ChainId in Oracle result is not uint64_t" + oracleResult);

    chainId = d["cid"].GetUint64();

    CHECK_STATE2(d.HasMember("uri"), "No URI in Oracle  result:" + oracleResult);

    CHECK_STATE2(d["uri"].IsString(), "Uri in Oracle result is not string:" + oracleResult);

    uri = d["uri"].GetString();

    CHECK_STATE(uri.size() > 5);

    CHECK_STATE2(d.HasMember("jsps"), "No json pointer in Oracle result:" + oracleResult);

    CHECK_STATE2(d["jsps"].IsArray(), "Jsps in Oracle spec is not array:" + oracleResult);

    CHECK_STATE2(d.HasMember("time"), "No time pointer in Oracle result:" + oracleResult);

    CHECK_STATE2(d["time"].IsUint64(), "time in Oracle result is not uint64:" + oracleResult)

    requestTime = d["time"].GetUint64();

    CHECK_STATE(requestTime > 0);

    CHECK_STATE2(d.HasMember("sig"), "No sig in Oracle result:" + oracleResult);

    CHECK_STATE2(d["sig"].IsString(), "sig in Oracle result is not string:" + oracleResult)

    sig = d["sig"].GetString();

    auto array = d["jsps"].GetArray();

    for (auto &&item: array) {
        CHECK_STATE2(item.IsString(), "Jsp array item is not string:" + oracleResult);
        string jsp  = item.GetString();
        CHECK_STATE2(!jsp.empty() && jsp.front() == '/', "Invalid JSP pointer:" + jsp);
        jsps.push_back(jsp);
    }

    if (d.HasMember("trims")) {
        auto trimArray = d["trims"].GetArray();
        for (auto &&item: trimArray) {
            CHECK_STATE2(item.IsUint64(), "Trims array item is not uint64 :" + oracleResult);
            trims.push_back(item.GetUint64());
        }

        CHECK_STATE2(jsps.size() == trims.size(), "jsps array size not equal trims array size");
    } else {
        for (uint64_t i = 0; i < jsps.size(); i++) {
            trims.push_back(0);
        }
    }


    if (d.HasMember("err")) {
        CHECK_STATE2(d["err"].IsUint64(), "Error is not uint64_t");
        error = d["err"].GetUint64();
        return;
    }

    auto resultsArray = d["rslts"].GetArray();

    results = make_shared<vector<ptr<string>>>();
    for (auto &&item: resultsArray) {
        if (item.IsString()) {
            results->push_back(make_shared<string>(item.GetString()));
        } else if (item.IsNull()) {
            results->push_back(nullptr);
        } else {
            CHECK_STATE2(false, "Unknown item in results:" + oracleResult)
        }
    }

    if (d.HasMember("post")) {
        CHECK_STATE2(d["post"].IsString(), "Post in Oracle result is not string:" + oracleResult);
        post = d["post"].GetString();
    }
}




OracleResult::OracleResult(string &_result, string& _encoding) : oracleResult(_result) {


    // now encode and sign result.
    if (_encoding == "rlp") {
        parseResultAsRlp();
    } else {
        parseResultAsJson();
    }

    if (this->isGeth()) {
        rapidjson::Document d2;
        d2.Parse(post.data());
        CHECK_STATE2(!d2.HasParseError(), "Unparsable geth Oracle post:" + post);

        CHECK_STATE2(d2.HasMember("method"), "No JSON-RPC method in geth Oracle post:" + post);

        CHECK_STATE2(d2["method"].IsString(), "method in Oracle post is not string:" + post)

        auto meth = d2["method"].GetString();

        if (meth == string("eth_call") ||
            meth == string("eth_gasPrice") || meth == string("eth_blockNumber") ||
            meth == string("eth_getBlockByNumber") ||
            meth == string("eth_getBlockByHash")) {}
        else {
            CHECK_STATE2(false, "Geth Method not allowed:" + meth);
        }
    }


    CHECK_STATE2(results->size() == trims.size(), "hsps array size not equal trims array size");

    RLPOutputStream stream(6);
    stream.append(chainId); //1
    stream.append(uri);//2
    stream.append(requestTime); //3
    stream.append(jsps); // 4
    stream.append(trims); //5
    stream.append(post); //6




    auto rlpEncoding = stream.out();
    auto hex = Utils::carray2Hex(rlpEncoding.data(), rlpEncoding.size());
    cerr << "Oracle result" << hex << endl;
    sleep(3);


}

const string &OracleResult::getPost() const {
    return post;
}


const string &OracleResult::getOracleResult() const {
    return oracleResult;
}

uint64_t OracleResult::getError() const {
    return error;
}

const ptr<vector<ptr<string>>> OracleResult::getResults() const {
    return results;
}

const string &OracleResult::toString() const {
    return oracleResult;
}

const string &OracleResult::getUri() const {
    return uri;
}


uint64_t OracleResult::getTime() const {
    return requestTime;
}


const vector<string> &OracleResult::getJsps() const {
    return jsps;
}

const vector<uint64_t> &OracleResult::getTrims() const {
    return trims;
}

ptr<OracleResult> OracleResult::parseResult(string &_oracleResult, string& _encoding) {
    return make_shared<OracleResult>(_oracleResult, _encoding);
}

const string &OracleResult::getSig() const {
    return sig;
}

uint64_t OracleResult::getChainId() const {
    return chainId;
}


bool OracleResult::isGeth() {
    return (uri.find("geth://") == 0);
}

void OracleResult::trimResults() {
    CHECK_STATE(results->size() == trims.size())
    for (uint64_t i = 0; i < results->size(); i++) {
        auto trim = trims.at(i);
        auto res = results->at(i);
        if (res && trim != 0) {
            if (res->size() <= trim) {
                res = make_shared<string>("");
            } else {
                res = make_shared<string>(res->substr(0, res->size() - trim));
            }
            (*results)[i] = res;
        }

    }
}

void OracleResult::appendElementsFromTheSpecAsJson() {
    oracleResult = "{";
    oracleResult.append(string("\"cid\":") + to_string(chainId) + ",");
    oracleResult.append(string("\"uri\":\"") + uri + "\",");
    oracleResult.append(string("\"jsps\":["));

    for (uint64_t j = 0; j < jsps.size(); j++) {
        oracleResult.append("\"");
        oracleResult.append(jsps.at(j));
        oracleResult.append("\"");
        if (j + 1 < jsps.size())
            oracleResult.append(",");
    }


    oracleResult.append("],");
    oracleResult.append("\"trims\":[");

    for (uint64_t j = 0; j < trims.size(); j++) {
        oracleResult.append(to_string(trims.at(j)));
        if (j + 1 < trims.size())
            oracleResult.append(",");
    }

    oracleResult.append("],");
    oracleResult.append(string("\"time\":") + to_string(requestTime) + ",");

    if (!post.empty()) {
        oracleResult.append(string("\"post\":") + post + ",");
    }
}

void OracleResult::appendElementsFromTheSpecAsRlp() {
    oracleResult = "{";
    oracleResult.append(string("\"cid\":") + to_string(chainId) + ",");
    oracleResult.append(string("\"uri\":\"") + uri + "\",");
    oracleResult.append(string("\"jsps\":["));

    for (uint64_t j = 0; j < jsps.size(); j++) {
        oracleResult.append("\"");
        oracleResult.append(jsps.at(j));
        oracleResult.append("\"");
        if (j + 1 < jsps.size())
            oracleResult.append(",");
    }


    oracleResult.append("],");
    oracleResult.append("\"trims\":[");

    for (uint64_t j = 0; j < trims.size(); j++) {
        oracleResult.append(to_string(trims.at(j)));
        if (j + 1 < trims.size())
            oracleResult.append(",");
    }

    oracleResult.append("],");
    oracleResult.append(string("\"time\":") + to_string(requestTime) + ",");

    if (!post.empty()) {
        oracleResult.append(string("\"post\":") + post + ",");
    }
}

void OracleResult::appendResultsAsJson() {
    // append results
    oracleResult.append("\"rslts\":[");

    for (uint64_t i = 0; i < results->size(); i++) {
        if (i != 0) {
            oracleResult.append(",");
        }

        if (results->at(i)) {
            oracleResult.append("\"");
            oracleResult.append(*results->at(i));
            oracleResult.append("\"");
        } else {
            oracleResult.append("null");
        }

    }

    oracleResult.append("],");/**/
}


void OracleResult::appendResultsAsRlp() {
    // append results
    oracleResult.append("\"rslts\":[");

    for (uint64_t i = 0; i < results->size(); i++) {
        if (i != 0) {
            oracleResult.append(",");
        }

        if (results->at(i)) {
            oracleResult.append("\"");
            oracleResult.append(*results->at(i));
            oracleResult.append("\"");
        } else {
            oracleResult.append("null");
        }

    }

    oracleResult.append("],");/**/
}



void OracleResult::signResultAsJson(ptr<CryptoManager> _cryptoManager) {
    CHECK_ARGUMENT(_cryptoManager)
    CHECK_STATE(oracleResult.at(oracleResult.size() - 1) == ',')
    sig = _cryptoManager->signOracleResult(oracleResult);
    oracleResult.append("\"sig\":\"");
    oracleResult.append(sig);
    oracleResult.append("\"}");
}

void OracleResult::signResultAsRlp(ptr<CryptoManager> _cryptoManager) {
    CHECK_ARGUMENT(_cryptoManager)
    CHECK_STATE(oracleResult.at(oracleResult.size() - 1) == ',')
    sig = _cryptoManager->signOracleResult(oracleResult);
    oracleResult.append("\"sig\":\"");
    oracleResult.append(sig);
    oracleResult.append("\"}");
}


void OracleResult::appendErrorAsJson() {
    oracleResult.append("\"err\":");
    oracleResult.append(to_string(error));
    oracleResult.append(",");

}

void OracleResult::appendErrorAsRlp() {
    oracleResult.append("\"err\":");
    oracleResult.append(to_string(error));
    oracleResult.append(",");

}

OracleResult::OracleResult(ptr<OracleRequestSpec> _oracleSpec, uint64_t _status, string &_serverResponse,
                           ptr<CryptoManager> _cryptoManager) {



    CHECK_ARGUMENT(_oracleSpec);



    chainId = _oracleSpec->getChainid();
    uri = _oracleSpec->getUri();
    jsps = _oracleSpec->getJsps();
    trims = _oracleSpec->getTrims();
    requestTime = _oracleSpec->getTime();
    post = _oracleSpec->getPost();
    error = _status;

    if (_status == ORACLE_SUCCESS) {
        results = extractResults(_serverResponse);
    }

    if (!results) {
        error = ORACLE_INVALID_JSON_RESPONSE;
    } else {
        trimResults();
    }


    // now encode and sign result.
    if (_oracleSpec->getEncoding() == "rlp") {
        encodeAndSignResultAsRlp(_cryptoManager);
    } else {
        encodeAndSignResultAsJson(_cryptoManager);
    }

}

void OracleResult::encodeAndSignResultAsJson(ptr<CryptoManager> _cryptoManager) {

    appendElementsFromTheSpecAsJson();

    if (error != ORACLE_SUCCESS) {
        appendErrorAsJson();
    } else {
        appendResultsAsJson();
    }

    signResultAsJson(_cryptoManager);

}

void OracleResult::encodeAndSignResultAsRlp(ptr<CryptoManager> _cryptoManager) {

    appendElementsFromTheSpecAsRlp();

    if (error != ORACLE_SUCCESS) {
        appendErrorAsRlp();
    } else {
        appendResultsAsRlp();
    }

    signResultAsRlp(_cryptoManager);
}



ptr<vector<ptr<string>>> OracleResult::extractResults(
        string &_response) {


    auto rs = make_shared<vector<ptr<string>>>();


    try {

        auto j = nlohmann::json::parse(_response);
        for (auto &&jsp: jsps) {
            auto pointer = nlohmann::json::json_pointer(jsp);
            try {
                auto val = j.at(pointer);
                CHECK_STATE(val.is_primitive());
                string strVal;
                if (val.is_string()) {
                    strVal = val.get<string>();
                } else if (val.is_number_integer()) {
                    if (val.is_number_unsigned()) {
                        strVal = to_string(val.get<uint64_t>());
                    } else {
                        strVal = to_string(val.get<int64_t>());
                    }
                } else if (val.is_number_float()) {
                    strVal = to_string(val.get<double>());
                } else if (val.is_boolean()) {
                    strVal = to_string(val.get<bool>());
                }
                rs->push_back(make_shared<string>(strVal));
            } catch (...) {
                rs->push_back(nullptr);
            }
        }

    } catch (exception& _e) {
        return nullptr;
    }
    catch (...) {
        return nullptr;
    }

    return rs;
}

const string OracleResult::getUnsignedOracleResultStr() const {
    auto commaPosition = oracleResult.find_last_of(",");
    CHECK_STATE(commaPosition != string::npos);
    auto res = oracleResult.substr(0, commaPosition + 1);
    return res;
}