//
// Created by kladko on 11.01.22.
//

#include "thirdparty/rapidjson/document.h"
#include "thirdparty/json.hpp"
#include "thirdparty/rapidjson/prettywriter.h" // for stringify JSON

#include "SkaleCommon.h"
#include "Log.h"

#include "crypto/CryptoManager.h"
#include "headers/BasicHeader.h"
#include "network/Utils.h"

#include "rlp/RLP.h"

#include "OracleRequestSpec.h"


ptr<OracleRequestSpec> OracleRequestSpec::parseSpec(const string &_spec) {
    return make_shared<OracleRequestSpec>(_spec);
}

OracleRequestSpec::OracleRequestSpec(const string &_spec) : spec(_spec) {
    rapidjson::Document d;
    spec.erase(std::remove_if(spec.begin(), spec.end(), ::isspace), spec.end());
    d.Parse(spec.data());
    ORACLE_CHECK_STATE2(!d.HasParseError(), "Unparsable Oracle spec:" + _spec);

    ORACLE_CHECK_STATE2(d.HasMember("cid"), "No chainid in Oracle spec:" + _spec);


    RLPStream stream(1);


    ORACLE_CHECK_STATE2(d["cid"].IsUint64(), "ChainId in Oracle spec is not uint64_t" + _spec);

    chainid = d["cid"].GetUint64();
    stream.append(chainid);

    ORACLE_CHECK_STATE2(d.HasMember("uri"), "No URI in Oracle spec:" + _spec);

    ORACLE_CHECK_STATE2(d["uri"].IsString(), "Uri in Oracle spec is not string:" + _spec);

    uri = d["uri"].GetString();

    //stream.append(uri);

    ORACLE_CHECK_STATE(uri.size() > 5);

    ORACLE_CHECK_STATE2(d.HasMember("jsps"), "No json pointer in Oracle spec:" + _spec);

    ORACLE_CHECK_STATE2(d["jsps"].IsArray(), "Jsps in Oracle spec is not array:" + _spec);

    ORACLE_CHECK_STATE2(d.HasMember("time"), "No time pointer in Oracle spec:" + _spec);

    ORACLE_CHECK_STATE2(d["time"].IsUint64(), "time in Oracle spec is not uint64:" + _spec)

    time = d["time"].GetUint64();

    //stream.append(time);

    ORACLE_CHECK_STATE(time > 0);

    ORACLE_CHECK_STATE2(d.HasMember("pow"), "No  pow in Oracle spec:" + _spec);

    ORACLE_CHECK_STATE2(d["pow"].IsUint64(), "Pow in Oracle spec is not uint64:" + _spec);

    //pow = d["pow"].GetUint64();

    //stream.append(pow);

    auto array = d["jsps"].GetArray();

    for (auto &&item: array) {
        ORACLE_CHECK_STATE2(item.IsString(), "Jsp array item is not string:" + _spec);
        jsps.push_back(item.GetString());
    }

    if (d.HasMember("trims")) {
        auto trimArray = d["trims"].GetArray();
        for (auto &&item: trimArray) {
            ORACLE_CHECK_STATE2(item.IsUint64(), "Trims array item is uint64 :" + _spec);
            trims.push_back(item.GetUint64());
        }

        ORACLE_CHECK_STATE2(jsps.size() == trims.size(), "hsps array size not equal trims array size");
    } else {
        for (uint64_t i = 0; i < jsps.size(); i++) {
            trims.push_back(0);
        }
    }

    if (d.HasMember("post")) {
        isPost = true;
        ORACLE_CHECK_STATE2(d["post"].IsString(), "Post in Oracle spec is not string:" + _spec);
        postStr = d["post"].GetString();
    }


    //stream.append((uint64_t)isPost);

    if (this->isGeth()) {
        rapidjson::Document d2;
        postStr.erase(std::remove_if(postStr.begin(), postStr.end(), ::isspace), postStr.end());
        d2.Parse(postStr.data());
        ORACLE_CHECK_STATE2(!d2.HasParseError(), "Unparsable geth Oracle post:" + postStr);

        ORACLE_CHECK_STATE2(d2.HasMember("method"), "No JSON-RPC method in geth Oracle post:" + postStr);

        ORACLE_CHECK_STATE2(d2["method"].IsString(), "method in Oracle post is not string:" + postStr)

        auto meth  = d2["method"].GetString();

        if (meth == string("eth_call") ||
            meth == string("eth_gasPrice") || meth == string("eth_blockNumber") ||
                    meth == string("eth_getBlockByNumber") ||
                    meth == string("eth_getBlockByHash") ) {} else {
            ORACLE_CHECK_STATE2(false, "Geth Method not allowed:" + meth);
        }
    }


    auto encodedChainId = stream.out();
    auto hex = Utils::carray2Hex(encodedChainId.data(), encodedChainId.size());
    cerr << hex;

    exit(75);
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

const uint64_t &OracleRequestSpec::getPow() const {
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

string OracleRequestSpec::getReceipt() {
    return CryptoManager::hashForOracle(spec);
}



bool OracleRequestSpec::verifyPow() {

    try {

        auto hash = CryptoManager::hashForOracle(spec);

        u256 binaryHash("0x" + hash);
`
        if (~u256(0) / binaryHash > u256(10000)) {
            return true;
        } {
            return false;
        }

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


