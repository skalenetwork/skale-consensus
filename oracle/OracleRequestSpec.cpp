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

    rapidjson::Document d;

    d.Parse(_spec.data());

    if (!d.HasMember("uri")) {
        LOG(err, "No URI in Oracle spec");
        return nullptr;
    }

    if (!d["uri"].IsString()) {
        LOG(err, "Uri in Oracle spec is not string");
    }

    if (!d.HasMember("jsp")) {
        LOG(err, "No json pointer in Oracle spec");
        return nullptr;
    }

    if (!d["jsp"].IsString()) {
        LOG(err, "Jsp in Oracle spec is not string");
    }

    if (!d.HasMember("jsp")) {
        LOG(err, "No json pointer in Oracle spec");
        return nullptr;
    }

    if (!d.HasMember("time")) {
        LOG(err, "No json pointer in Oracle spec");
        return nullptr;
    }

    if (!d["time"].IsInt64()) {
        LOG(err, "time in Oracle spec is not int64");
        return nullptr;
    }

    return make_shared<OracleRequestSpec>(_spec);
}

OracleRequestSpec::OracleRequestSpec(string& )  {}