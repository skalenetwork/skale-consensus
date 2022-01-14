//
// Created by kladko on 14.01.22.
//

#include "thirdparty/rapidjson/document.h"
#include "thirdparty/json.hpp"
#include "thirdparty/rapidjson/prettywriter.h" // for stringify JSON


#include "SkaleCommon.h"
#include "Log.h"
#include "OracleResult.h"



OracleResult::OracleResult(string &_result) : oracleResult(_result) {
    rapidjson::Document d;
    
    d.Parse(_result.data());

    CHECK_STATE2(!d.HasParseError(), "Unparsable Oracle spec:" + _result);
    
    CHECK_STATE2(d.HasMember("uri"), "No URI in Oracle spec:" + _result);

    CHECK_STATE2(d["uri"].IsString(), "Uri in Oracle spec is not string:" + _result);

    uri = d["uri"].GetString();

    CHECK_STATE(uri.size() > 5);

    CHECK_STATE2(d.HasMember("jsps"), "No json pointer in Oracle spec:" + _result);

    CHECK_STATE2(d["jsps"].IsArray(), "Jsps in Oracle spec is not array:" + _result);

    CHECK_STATE2(d.HasMember("time"), "No time pointer in Oracle spec:" + _result);

    CHECK_STATE2(d["time"].IsUint64(), "time in Oracle spec is not uint64:" + _result)

    time = d["time"].GetUint64();

    CHECK_STATE(time > 0);

    auto array = d["jsps"].GetArray();

    for (auto &&item: array) {
        CHECK_STATE2(item.IsString(), "Jsp array item is not string:" + _result);
        jsps.push_back(item.GetString());
    }

    if (d.HasMember("trims")) {
        auto trimArray = d["trims"].GetArray();
        for (auto &&item: trimArray) {
            CHECK_STATE2(item.IsUint64(), "Trims array item is not uint64 :" + _result);
            trims.push_back(item.GetUint64());
        }

        CHECK_STATE2(jsps.size() == trims.size(), "hsps array size not equal trims array size");
    } else {
        for (uint64_t i = 0; i < jsps.size(); i++) {
            trims.push_back(0);
        }
    }

    time = d["time"].GetUint64();


    if (d.HasMember("err")) {
        CHECK_STATE2(d["err"].IsUint64(), "Error is not uint65_t");
        error = d["err"].GetUint64();
        return;
    }

    auto resultsArray = d["rslts"].GetArray();
    for (auto &&item: resultsArray) {
        if (item.IsString()) {
            results.push_back(make_shared<string>(item.GetString()));
        } else if (item.IsNull()) {
            results.push_back(nullptr);
        } else {
            CHECK_STATE2(false, "Unknown item in results:" + _result)
        }
    }

    CHECK_STATE2(results.size() == trims.size(), "hsps array size not equal trims array size");


}

const string &OracleResult::getOracleResult() const {
    return oracleResult;
}

uint64_t OracleResult::getError() const {
    return error;
}

const vector<ptr<string>> &OracleResult::getResults() const {
    return results;
}

const string &OracleResult::getResult() const {
    return oracleResult;
}

const string &OracleResult::getUri() const {
    return uri;
}


uint64_t OracleResult::getTime() const {
    return time;
}


const vector<string> &OracleResult::getJsps() const {
    return jsps;
}

const vector<uint64_t> &OracleResult::getTrims() const {
    return trims;
}

ptr<OracleResult> OracleResult::parseResult(string &_oracleResult) {
    return make_shared<OracleResult>(_oracleResult);
}

