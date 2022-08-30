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

    requestTime = d["time"].GetUint64();

    CHECK_STATE(requestTime > 0);

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


    if (d.HasMember("err")) {
        CHECK_STATE2(d["err"].IsUint64(), "Error is not uint64_t");
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

    if (d.HasMember("post")) {
        CHECK_STATE2(d["post"].IsString(), "Post in Oracle result is not string:" + _result);
        post = d["post"].GetString();
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


    CHECK_STATE2(results.size() == trims.size(), "hsps array size not equal trims array size");

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
    exit(75);


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
    return requestTime;
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

OracleResult::OracleResult(uint64_t _chainId, const string &_uri,
                           const vector<string> &_jsps, const vector<uint64_t> &_trims, uint64_t _time,
                           const string &_post) :
        chainId(_chainId),
        uri(_uri),
        jsps(_jsps),
        trims(_trims),
        requestTime(_time),
        post(_post) {
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