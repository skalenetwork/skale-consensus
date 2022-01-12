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

ptr<OracleRequestSpec> OracleRequestSpec::parseSpec(string& _spec) {

    return make_shared<OracleRequestSpec>(_spec);
}

OracleRequestSpec::OracleRequestSpec(string& _spec ) : spec(_spec) {
    rapidjson::Document d;

    spec.erase(std::remove_if(spec.begin(), spec.end(), ::isspace), spec.end());

    d.Parse(spec.data());

    CHECK_STATE2(!d.HasParseError(), "Unparsable Oracle spec:" + _spec);

    CHECK_STATE2(d.HasMember("uri"),"No URI in Oracle spec:" + _spec);

    CHECK_STATE2(d["uri"].IsString(), "Uri in Oracle spec is not string:" + _spec);

    CHECK_STATE2(d.HasMember("jsp"), "No json pointer in Oracle spec:" + _spec);

    CHECK_STATE2(d["jsp"].IsString(), "Jsp in Oracle spec is not string:" + _spec);

    CHECK_STATE2(d.HasMember("jsp"), "No json pointer in Oracle spec:" + _spec);

    CHECK_STATE2(d.HasMember("time"), "No time pointer in Oracle spec:" + _spec);

    CHECK_STATE2(d["time"].IsUint64(),"time in Oracle spec is not uint64:" + _spec)

    CHECK_STATE2(d.HasMember("pow"), "No  pow in Oracle spec:" + _spec);

    CHECK_STATE2(d["pow"].IsString(), "Pow in Oracle spec is not string:" + _spec);

    uri = d["uri"].GetString();
    jsp = d["jsp"].GetString();
    time = d["time"].GetUint64();
    pow = d["pow"].GetString();

}

const string &OracleRequestSpec::getSpec() const {
    return spec;
}

const string &OracleRequestSpec::getUri() const {
    return uri;
}

const string &OracleRequestSpec::getJsp() const {
    return jsp;
}

uint64_t OracleRequestSpec::getTime() const {
    return time;
}

const string &OracleRequestSpec::getPow() const {
    return pow;
}
