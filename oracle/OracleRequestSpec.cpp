//
// Created by kladko on 11.01.22.
//

#include "thirdparty/rapidjson/document.h"
#include "thirdparty/json.hpp"
#include "thirdparty/rapidjson/prettywriter.h" // for stringify JSON

#include "SkaleCommon.h"
#include "Log.h"

#include "headers/BasicHeader.h"

#include "OracleRequestSpec.h"

ptr<OracleRequestSpec> OracleRequestSpec::parseSpec(string &_spec) {
    return make_shared<OracleRequestSpec>(_spec);
}

OracleRequestSpec::OracleRequestSpec(string &_spec) : spec(_spec) {
    rapidjson::Document d;

    spec.erase(std::remove_if(spec.begin(), spec.end(), ::isspace), spec.end());

    d.Parse(spec.data());

    CHECK_STATE2(d.HasMember("cid"), "No chainid in Oracle spec:" + _spec);

    CHECK_STATE2(d["cid"].IsUint64(), "ChainId in Oracle spec is not uint64_t" + _spec);

    chainid = d["cid"].GetUint64();

    CHECK_STATE2(!d.HasParseError(), "Unparsable Oracle spec:" + _spec);

    CHECK_STATE2(d.HasMember("uri"), "No URI in Oracle spec:" + _spec);

    CHECK_STATE2(d["uri"].IsString(), "Uri in Oracle spec is not string:" + _spec);

    uri = d["uri"].GetString();

    CHECK_STATE(uri.size() > 5);

    CHECK_STATE2(d.HasMember("jsps"), "No json pointer in Oracle spec:" + _spec);

    CHECK_STATE2(d["jsps"].IsArray(), "Jsps in Oracle spec is not array:" + _spec);

    CHECK_STATE2(d.HasMember("time"), "No time pointer in Oracle spec:" + _spec);

    CHECK_STATE2(d["time"].IsUint64(), "time in Oracle spec is not uint64:" + _spec)

    time = d["time"].GetUint64();

    CHECK_STATE(time > 0);

    CHECK_STATE2(d.HasMember("pow"), "No  pow in Oracle spec:" + _spec);

    CHECK_STATE2(d["pow"].IsString(), "Pow in Oracle spec is not string:" + _spec);

    pow = d["pow"].GetString();

    CHECK_STATE(pow.size() > 4);

    auto array = d["jsps"].GetArray();

    for (auto &&item: array) {
        CHECK_STATE2(item.IsString(), "Jsp array item is not string:" + _spec);
        jsps.push_back(item.GetString());
    }

    if (d.HasMember("trims")) {
        auto trimArray = d["trims"].GetArray();
        for (auto &&item: trimArray) {
            CHECK_STATE2(item.IsUint64(), "Trims array item is uint64 :" + _spec);
            trims.push_back(item.GetUint64());
        }

        CHECK_STATE2(jsps.size() == trims.size(), "hsps array size not equal trims array size");
    } else {
        for (uint64_t i = 0; i < jsps.size(); i++) {
            trims.push_back(0);
        }
    }

    if (d.HasMember("post")) {
        isPost = true;
        CHECK_STATE2(d["post"].IsString(), "Pow in Oracle spec is not string:" + _spec);
        postStr = d["post"].GetString();
    }


        time = d["time"].GetUint64();
    pow = d["pow"].GetString();

}

const string &OracleRequestSpec::getSpec() const {
    return spec;
}

const string &OracleRequestSpec::getUri() const {
    return uri;
}


uint64_t OracleRequestSpec::getTime() const {
    return time;
}

const string &OracleRequestSpec::getPow() const {
    return pow;
}

const vector<string> &OracleRequestSpec::getJsps() const {
    return jsps;
}

const vector<uint64_t> &OracleRequestSpec::getTrims() const {
    return trims;
}

uint64_t OracleRequestSpec::getChainid() const {
    return chainid;
}

bool OracleRequestSpec::getPost() const {
    return isPost;
}

const string &OracleRequestSpec::getPostStr() const {
    return postStr;
}

bool OracleRequestSpec::isGeth() {
    return (uri.find("geth://") == 0);
}
