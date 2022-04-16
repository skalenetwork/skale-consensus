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

    CHECK_STATE2(!d.HasParseError(), "Unparsable Oracle result:" + _result);

    CHECK_STATE2(d.HasMember("cid"), "No chainid in Oracle  result:" + _result);

    CHECK_STATE2(d["cid"].IsUint64(), "ChainId in Oracle result is not uint64_t" + _result);

    chainId = d["cid"].GetUint64();

    CHECK_STATE2(d.HasMember("uri"), "No URI in Oracle  result:" + _result);

    CHECK_STATE2(d["uri"].IsString(), "Uri in Oracle result is not string:" + _result);

    uri = d["uri"].GetString();

    CHECK_STATE(uri.size() > 5);

    CHECK_STATE2(d.HasMember("jsps"), "No json pointer in Oracle result:" + _result);

    CHECK_STATE2(d["jsps"].IsArray(), "Jsps in Oracle spec is not array:" + _result);

    CHECK_STATE2(d.HasMember("time"), "No time pointer in Oracle result:" + _result);

    CHECK_STATE2(d["time"].IsUint64(), "time in Oracle result is not uint64:" + _result)

    time = d["time"].GetUint64();

    CHECK_STATE(time > 0);

    CHECK_STATE2(d.HasMember("sig"), "No sig in Oracle result:" + _result);

    CHECK_STATE2(d["sig"].IsString(), "sig in Oracle result is not string:" + _result)

    sig = d["sig"].GetString();

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


    CHECK_STATE2(d.HasMember("status"), "No status in Oracle result:" + _result);
    CHECK_STATE2(d["status"].IsUint64(), "status is not uint64_t");
    status = d["status"].GetUint64();

    if (status > 0) {
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

    if (d.HasMember("post")) {
        isPost = true;
        CHECK_STATE2(d["post"].IsString(), "Post in Oracle result is not string:" + _result);
        postStr = d["post"].GetString();
    }


    if (this->isGeth()) {
        rapidjson::Document d2;
        postStr.erase(std::remove_if(postStr.begin(), postStr.end(), ::isspace), postStr.end());
        d2.Parse(postStr.data());
        CHECK_STATE2(!d2.HasParseError(), "Unparsable geth Oracle post:" + postStr);

        CHECK_STATE2(d2.HasMember("method"), "No JSON-RPC method in geth Oracle post:" + postStr);

        CHECK_STATE2(d2["method"].IsString(), "method in Oracle post is not string:" + postStr)

        auto meth  = d2["method"].GetString();

        if (meth == string("eth_call") ||
            meth == string("eth_gasPrice") || meth == string("eth_blockNumber") ||
            meth == string("eth_getBlockByNumber") ||
            meth == string("eth_getBlockByHash") ) {} else {
            CHECK_STATE2(false, "Geth Method not allowed:" + meth);
        }
    }


    CHECK_STATE2(results.size() == trims.size(), "hsps array size not equal trims array size");


}

bool OracleResult::getPost() const {
    return isPost;
}

const string &OracleResult::getPostStr() const {
    return postStr;
}

const string &OracleResult::getOracleResult() const {
    return oracleResult;
}

uint64_t OracleResult::getStatus() const {
    return status;
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

const string &OracleResult::getSig() const {
    return sig;
}

uint64_t OracleResult::getChainId() const {
    return chainId;
}


bool OracleResult::isGeth() {
    return (uri.find("geth://") == 0);
}

