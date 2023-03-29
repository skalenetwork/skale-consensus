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
    CHECK_STATE2(_encoding == "json",
    /// || _encoding == "abi",
                 "Unknown encoding " + encoding);
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
    if (_uri != "eth://") {
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

        CHECK_STATE2(d.HasMember("encoding"), "No encoding in Oracle spec:" + _spec);

        CHECK_STATE2(d["encoding"].IsString(), "Encoding in Oracle spec is not string:" + _spec);
        encoding = d["encoding"].GetString();
        checkEncoding(encoding);


        if (isEthApi()) {
            parseEthApiRequestSpec(d, _spec);
        } else {
            CHECK_STATE2(uri != "eth://", "No valid eth API method is provided for eth:// URI");
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
    CHECK_STATE2(!d.HasMember("jsps"), "jsps element should not be present in eth_call request" + _spec);

    CHECK_STATE2(!d.HasMember("trims"), "trims element should not be present in eth_call request" + _spec);

    CHECK_STATE2(d.HasMember("params"),
                 "eth_call request shall include params element, which could be an empty array" + _spec);

    CHECK_STATE2(d["params"].IsArray(), "eth_call request shall include params element, which could be an empty array"
                                        + _spec);

    auto params = d["params"].GetArray();

    CHECK_STATE2(params.Size() == 2, "Params array size must be 2 " + _spec);

    CHECK_STATE2(params[0].IsObject(), "The first element in params array must be object");
    CHECK_STATE2(params[0].HasMember("to"), "The first element in params array must include to field");
    CHECK_STATE2(params[0].HasMember("from"), "The first element in params array must include from field");
    CHECK_STATE2(params[0].HasMember("data"), "The first element in params array must include data field");
    CHECK_STATE2(params[0].MemberCount() == 3, "The first element in params array must be three key value pairs"
                                               " from, to, and data");
    CHECK_STATE2(params[1].IsString(), "The second element in params array must be string");


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
                                                   const string &_post, const string &_ethApi,
                                                   const string &_from, const string &_to, const string &_data,
                                                   const string &_gas, const string &_blockId,
                                                   const string &_encoding,
                                                   uint64_t _time) {


    string spec;

    try {

        // iterate over pow until you get the correct number
        for (uint64_t pow = 0;; pow++) {
            spec = tryMakingSpec(_chainId, _uri, _jsps, _trims, _post, _ethApi,
                                 _from, _to, _data, _gas, _blockId, _encoding, _time, pow);
            if (verifyPow(spec)) {
                // found the correct value of pow. return spec object
                return make_shared<OracleRequestSpec>(spec);
            }
        }

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}

string OracleRequestSpec::tryMakingSpec(uint64_t _chainId, const string &_uri, const vector<string> &_jsps,
                                        const vector<uint64_t> &_trims, const string &_post,
                                        const string &_ethApi,
                                        const string &_from, const string &_to, const string &_data, const string& _gas,
                                        const string & _blockId,
                                        const string &_encoding,
                                        uint64_t _time,
                                        uint64_t _pow) {
    auto specStr = makeSpecStart(_chainId, _uri);

    if (_ethApi.empty()) {
        // web spec
        appendWebPart(specStr, _jsps, _trims, _post);
    } else {
        // ethApi
        CHECK_STATE(_ethApi == "eth_call")
        specStr.append(string(+"\"ethApi\":\"") + _ethApi + "\",");
        appendEthCallPart(specStr, _from, _to, _data, _gas, _blockId);
    }

    appendSpecEnd(specStr, _encoding, _time, _pow);

    return specStr;
}

void
OracleRequestSpec::appendEthCallPart( string &_specStr,
        const string &_from, const string &_to, const string &_gas, const string &_data,
        const string& _blockId) {
    _specStr.append("\"params\":[{");
    _specStr.append("\"from\":\"" + _from + "\",");
    _specStr.append("\"to\":\"" + _to + "\",");
    _specStr.append("\"data\":\"" + _data + "\",");
    _specStr.append("\"gas\":\"" + _gas  );
    _specStr.append("},\"" + _blockId + "\"");
    _specStr.append("]");
}

void
OracleRequestSpec::appendWebPart(string &_specStr, const vector<string> &_jsps, const vector<uint64_t> &_trims,
                                 const string &_post) {
    _specStr.append(string("\"jsps\":["));

    for (uint64_t j = 0; j < _jsps.size(); j++) {
        _specStr.append("\"");
        string jsp = _jsps.at(j);
        _specStr.append(jsp);
        _specStr.append("\"");

        // dont append comma to the last element
        if (j + 1 < _jsps.size()) {
            _specStr.append(",");
        }
    }

    _specStr.append("],");

    if (_trims.size() > 0) {

        CHECK_STATE(_trims.size() == _jsps.size());

        _specStr.append("\"trims\":[");

        for (uint64_t j = 0; j < _trims.size(); j++) {
            _specStr.append(to_string(_trims.at(j)));
            // dont append comma to the last element
            if (j + 1 < _trims.size())
                _specStr.append(",");
        }

        _specStr.append("],");
    }

    if (!_post.empty()) {
        _specStr.append(string("\"post\":\"") + _post + "\",");
    }
}

void OracleRequestSpec::appendSpecEnd(string &specStr, const string &_encoding, uint64_t _time, uint64_t _pow) {
    specStr.append(string("\"encoding\":\"") + _encoding + "\",");
    specStr.append(string("\"time\":") + to_string(_time) + ",");
    specStr.append(string("\"pow\":") + to_string(_pow));
    specStr.append("}");
}

string OracleRequestSpec::makeSpecStart(uint64_t _chainId, const string &_uri) {
    string specStr("{");
    specStr.append(string("\"cid\":") + to_string(_chainId) + ",");
    specStr.append(string("\"uri\":\"") + _uri + "\",");
    return specStr;
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

ptr<OracleRequestSpec>
OracleRequestSpec::makeWebSpec(uint64_t _chainId, const string &_uri, const vector<string> &_jsps,
                               const vector<uint64_t> &_trims, const string &_post,
                               const string &_encoding, uint64_t _time) {
    return makeSpec(_chainId, _uri, _jsps, _trims, _post, "", "", "", "", "", "", _encoding, _time);
}

