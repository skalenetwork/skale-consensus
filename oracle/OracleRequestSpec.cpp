//
// Created by kladko on 11.01.22.
//


#include "thirdparty/json.hpp"
#include "thirdparty/rapidjson/prettywriter.h" // for stringify JSON
#include "thirdparty/LUrlParser.h"

#include "SkaleCommon.h"
#include "Log.h"

#include "crypto/CryptoManager.h"
#include "headers/BasicHeader.h"
#include "network/Utils.h"

#include "rlp/RLP.h"
#include "utils/Time.h"
#include "OracleRequestSpec.h"


ptr<OracleRequestSpec> OracleRequestSpec::parseSpec(const string &_spec, uint64_t _chainId) {
    try {
        auto spec = make_shared<OracleRequestSpec>(_spec);
        CHECK_STATE2(spec->getChainid() == _chainId,
                     "Invalid schain id in oracle spec:" + to_string(spec->getChainid()));

        CHECK_STATE2(spec->getTime() + ORACLE_TIMEOUT_MS > Time::getCurrentTimeMs(), "Request timeout")
        CHECK_STATE(spec->getTime() < Time::getCurrentTimeMs() + ORACLE_FUTURE_JITTER_MS);
        return spec;
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


void OracleRequestSpec::checkEncoding(const string &_encoding) {
    CHECK_STATE2(_encoding == "json" || _encoding == "abi", "Unknown encoding " + encoding);
}


void OracleRequestSpec::checkEthApi(const string &_ethApi) {
    if (_ethApi == string("eth_call")) {}
    else {
        CHECK_STATE2(false, "Eth Method is not supported:" + _ethApi);
    }
}

void OracleRequestSpec::checkURI(const string &_uri) {
    CHECK_STATE2(_uri.size() > 5, "Uri too short:" + _uri);
    CHECK_STATE2(_uri.size() <= ORACLE_MAX_URI_SIZE, "Uri too long:" + _uri);
    CHECK_STATE2(_uri.find(ORACLE_HTTP_START) == 0 ||
                 _uri.find(ORACLE_HTTPS_START == 0 ||
                           _uri == ORACLE_ETH_URL), "Invalid URI:" + _uri);
    if (_uri != "geth://") {
        auto result = LUrlParser::ParseURL::parseURL(uri);
        CHECK_STATE2(result.isValid(), "URL invalid:" + uri);
        CHECK_STATE2(result.userName_.empty(), "Non empty username");
        CHECK_STATE2(result.password_.empty(), "Non empty password");
        auto host = result.host_;

        CHECK_STATE2(host.find("0.") != 0 &&
                     host.find("10.") != 0 &&
                     host.find("127.") != 0 &&
                     host.find("172.") != 0 &&
                     host.find("192.168.") != 0 &&
                     host.find("169.254.") != 0 &&
                     host.find("192.0.0") != 0 &&
                     host.find("192.0.2") != 0 &&
                     host.find("192.0.2") != 0 &&
                     host.find("198.18") != 0 &&
                     host.find("198.19") != 0,
                     "Private IPs not allowed in Oracle:" + _uri
        )
    }
}

OracleRequestSpec::OracleRequestSpec(const string &_spec) : spec(_spec) {
    try {


        // generate receipt
        receipt = CryptoManager::hashForOracle(spec.data(), spec.size());

        // no parse

        rapidjson::Document d;
        d.Parse(spec.data());
        CHECK_STATE2(!d.HasParseError(), "Unparsable Oracle spec:" + _spec);


        // first check elements required for all calls

        CHECK_STATE2(d.HasMember("cid"), "No chainid in Oracle spec:" + _spec);
        CHECK_STATE2(d["cid"].IsUint64(), "ChainId in Oracle spec is not uint64_t" + _spec);
        chainid = d["cid"].GetUint64();

        CHECK_STATE2(d.HasMember("uri"), "No URI in Oracle spec:" + _spec);
        CHECK_STATE2(d["uri"].IsString(), "Uri in Oracle spec is not string:" + _spec);
        uri = d["uri"].GetString();
        checkURI(uri);

        CHECK_STATE2(d.HasMember("time"), "No time pointer in Oracle spec:" + _spec);
        CHECK_STATE2(d["time"].IsUint64(), "time in Oracle spec is not uint64:" + _spec)
        requestTime = d["time"].GetUint64();

        CHECK_STATE(requestTime > 0);

        CHECK_STATE2(d.HasMember("pow"), "No  pow in Oracle spec:" + _spec);
        CHECK_STATE2(d["pow"].IsUint64(), "Pow in Oracle spec is not uint64:" + _spec);
        pow = d["pow"].GetUint64();

        CHECK_STATE2(verifyPow(spec), "PoW did not verify");
        receipt = CryptoManager::hashForOracle(spec.data(), spec.size());

        // no check if ETH or WEB call

        if (d.HasMember("ethApi")) {
            CHECK_STATE2(d["ethApi"].IsString(), "ethAPI in Oracle spec is not string:" + _spec);
            ethApi = d["ethApi"].GetString();
            checkEthApi(ethApi);
        }

        if (d.HasMember("encoding")) {
            CHECK_STATE2(d["encoding"].IsString(), "Encoding in Oracle spec is not string:" + _spec);
            encoding = d["encoding"].GetString();
            checkEncoding(encoding);
        }


        if (isEthApi()) {
            parseEthApiRequestSpec(d, _spec);
        } else {
            parseWebRequestSpec(d, _spec);
        }

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


void OracleRequestSpec::parseWebRequestSpec(rapidjson::Document &d, const string &_spec) {
    CHECK_STATE2(d.HasMember("jsps"), "No json pointer in Oracle spec:" + _spec);
    CHECK_STATE2(d["jsps"].IsArray(), "Jsps in Oracle spec is not array:" + _spec);

    auto array = d["jsps"].GetArray();
    CHECK_STATE2(!array.Empty(), "Jsps array is empty.:" + _spec);
    CHECK_STATE2(array.Size() <= ORACLE_MAX_JSPS, "Too many elements in JSP array:" + _spec);

    for (auto &&item: array) {
        CHECK_STATE2(item.IsString(), "Jsps array item is not string:" + _spec);
        auto jsp = (string) item.GetString();
        CHECK_STATE2(jsp.size() <= ORACLE_MAX_JSP_SIZE, "JSP too long:" + _spec);
        jsps.push_back(jsp);
    }

    // now check optional elements


    if (d.HasMember("trims")) {
        auto trimArray = d["trims"].GetArray();
        for (auto &&item: trimArray) {
            CHECK_STATE2(item.IsUint64(), "Trims array item is uint64:" + _spec);
            trims.push_back(item.GetUint64());
        }
        CHECK_STATE2(jsps.size() == trims.size(), "hsps array size not equal tp trims array size:" + _spec);
    } else {
        for (uint64_t i = 0; i < jsps.size(); i++) {
            trims.push_back(0);
        }
    }

    if (d.HasMember("post")) {
        CHECK_STATE2(d["post"].IsString(), "Post in Oracle spec is not a string:" + _spec);
        post = d["post"].GetString();
        CHECK_STATE2(post.size() <= ORACLE_MAX_POST_SIZE, "Post string is larger than max allowed:" + _spec);
    }
}


void OracleRequestSpec::parseEthApiRequestSpec(rapidjson::Document &d, const string &_spec) {
    CHECK_STATE2(d.HasMember("jsps"), "No json pointer in Oracle spec:" + _spec);
    CHECK_STATE2(d["jsps"].IsArray(), "Jsps in Oracle spec is not array:" + _spec);

    auto array = d["jsps"].GetArray();
    CHECK_STATE2(!array.Empty(), "Jsps array is empty.:" + _spec);
    CHECK_STATE2(array.Size() <= ORACLE_MAX_JSPS, "Too many elements in JSP array:" + _spec);

    for (auto &&item: array) {
        CHECK_STATE2(item.IsString(), "Jsps array item is not string:" + _spec);
        auto jsp = (string) item.GetString();
        CHECK_STATE2(jsp.size() <= ORACLE_MAX_JSP_SIZE, "JSP too long:" + _spec);
        jsps.push_back(jsp);
    }

    // now check optional elements


    if (d.HasMember("trims")) {
        auto trimArray = d["trims"].GetArray();
        for (auto &&item: trimArray) {
            CHECK_STATE2(item.IsUint64(), "Trims array item is uint64:" + _spec);
            trims.push_back(item.GetUint64());
        }
        CHECK_STATE2(jsps.size() == trims.size(), "hsps array size not equal tp trims array size:" + _spec);
    } else {
        for (uint64_t i = 0; i < jsps.size(); i++) {
            trims.push_back(0);
        }
    }

    if (d.HasMember("post")) {
        CHECK_STATE2(d["post"].IsString(), "Post in Oracle spec is not a string:" + _spec);
        post = d["post"].GetString();
        CHECK_STATE2(post.size() <= ORACLE_MAX_POST_SIZE, "Post string is larger than max allowed:" + _spec);
    }
}

const string &OracleRequestSpec::getSpec() const {
    return spec;
}

const string &OracleRequestSpec::getUri() const {
    return uri;
}


uint64_t OracleRequestSpec::getTime() const {
    return requestTime;
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


const string &OracleRequestSpec::getPost() const {
    return post;
}


bool OracleRequestSpec::isPost() {
    return !post.empty();
}

bool OracleRequestSpec::isEthApi() {
    return (!ethApi.empty());
}

string OracleRequestSpec::getReceipt() {
    return receipt;
}


bool OracleRequestSpec::verifyPow(string &_spec) {

    try {

        auto hash = CryptoManager::hashForOracle(_spec.data(), _spec.size());

        u256 binaryHash("0x" + hash);

        if (~u256(0) / binaryHash > u256(10000)) {
            return true;
        }
        {
            return false;
        }

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}

ptr<OracleRequestSpec> OracleRequestSpec::makeSpec(uint64_t _chainId, const string &_uri,
                                                   const vector<string> &_jsps, const vector<uint64_t> &_trims,
                                                   uint64_t _time,
                                                   const string &_post, const string &_encoding) {

    string spec;

    try {

        for (::uint64_t pow = 0;; pow++) {

            spec = "{";

            spec.append(string("\"cid\":") + to_string(_chainId) + ",");
            spec.append(string("\"uri\":\"") + _uri + "\",");
            spec.append(string("\"jsps\":["));

            for (uint64_t j = 0; j < _jsps.size(); j++) {
                spec.append("\"");
                string jsp = _jsps.at(j);
                spec.append(jsp);
                spec.append("\"");
                if (j + 1 < _jsps.size())
                    spec.append(",");
            }


            spec.append("],");

            if (_trims.size() > 0) {

                CHECK_STATE(_trims.size() == _jsps.size());

                spec.append("\"trims\":[");

                for (uint64_t j = 0; j < _trims.size(); j++) {
                    spec.append(to_string(_trims.at(j)));
                    if (j + 1 < _trims.size())
                        spec.append(",");
                }

                spec.append("],");
            }
            spec.append(string("\"time\":") + to_string(_time) + ",");

            if (!_post.empty()) {
                spec.append(string("\"post\":\"") + _post + "\",");
            }

            if (!_encoding.empty()) {
                spec.append(string("\"encoding\":\"") + _encoding + "\",");
            }

            spec.append(string("\"pow\":") + to_string(pow));
            spec.append("}");

            if (verifyPow(spec)) {
                break;
            }
        }

        return make_shared<OracleRequestSpec>(spec);


    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}

const string &OracleRequestSpec::getEncoding() const {
    return encoding;
}


bool OracleRequestSpec::isEthMainnet() const {
    return uri == "eth://";
}

const string &OracleRequestSpec::getEthApi() const {
    return ethApi;
}

