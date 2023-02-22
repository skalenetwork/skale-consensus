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
    try {
        return make_shared<OracleRequestSpec>(_spec);
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}


void OracleRequestSpec::checkEncoding(const string & _encoding) {
    CHECK_STATE2(_encoding.empty() || _encoding == "json" || _encoding == "rlp", "Unknown encoding " + encoding);
}

OracleRequestSpec::OracleRequestSpec(const string &_spec) : spec(_spec) {
    try {
        rapidjson::Document d;
        d.Parse(spec.data());
        CHECK_STATE2(!d.HasParseError(), "Unparsable Oracle spec:" + _spec);

        CHECK_STATE2(d.HasMember("cid"), "No chainid in Oracle spec:" + _spec);


        CHECK_STATE2(d["cid"].IsUint64(), "ChainId in Oracle spec is not uint64_t" + _spec);

        chainid = d["cid"].GetUint64();

        CHECK_STATE2(d.HasMember("uri"), "No URI in Oracle spec:" + _spec);

        CHECK_STATE2(d["uri"].IsString(), "Uri in Oracle spec is not string:" + _spec);

        uri = d["uri"].GetString();

        CHECK_STATE(uri.size() > 5);

        CHECK_STATE(uri.size() <= ORACLE_MAX_URI_SIZE);

        CHECK_STATE2(d.HasMember("jsps"), "No json pointer in Oracle spec:" + _spec);

        CHECK_STATE2(d["jsps"].IsArray(), "Jsps in Oracle spec is not array:" + _spec);

        CHECK_STATE2(d.HasMember("time"), "No time pointer in Oracle spec:" + _spec);

        CHECK_STATE2(d["time"].IsUint64(), "time in Oracle spec is not uint64:" + _spec)

        requestTime = d["time"].GetUint64();


        CHECK_STATE(requestTime > 0);

        CHECK_STATE2(d.HasMember("pow"), "No  pow in Oracle spec:" + _spec);

        CHECK_STATE2(d["pow"].IsUint64(), "Pow in Oracle spec is not uint64:" + _spec);

        pow = d["pow"].GetUint64();

        auto array = d["jsps"].GetArray();


        CHECK_STATE2(!array.Empty(), "Jsps array is empty.:" + _spec);

        CHECK_STATE(array.Size() <= ORACLE_MAX_JSPS);

        for (auto &&item: array) {
            CHECK_STATE2(item.IsString(), "Jsps array item is not string:" + _spec);
            auto jsp = (string) item.GetString();
            CHECK_STATE(jsp.size() <= ORACLE_MAX_JSP_SIZE);
            jsps.push_back(jsp);
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
            CHECK_STATE2(d["post"].IsString(), "Post in Oracle spec is not a string:" + _spec);
            post = d["post"].GetString();
        }

        if (d.HasMember("encoding")) {
            CHECK_STATE2(d["encoding"].IsString(), "Encoding in Oracle spec is not string:" + _spec);
            encoding = d["encoding"].GetString();
        }


        checkEncoding(encoding);


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

        CHECK_STATE2(verifyPow(), "PoW did not verify");

        receipt = CryptoManager::hashForOracle(spec.data(), spec.size());
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
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

bool OracleRequestSpec::isGeth() {
    return (uri.find("geth://") == 0);
}

string OracleRequestSpec::getReceipt() {
    return receipt;
}


bool OracleRequestSpec::verifyPow() {

    try {

        auto hash = CryptoManager::hashForOracle(spec.data(), spec.size());

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

OracleRequestSpec::OracleRequestSpec(uint64_t _chainId, const string &_uri,
                                     const vector<string> &_jsps, const vector<uint64_t> &_trims, uint64_t _time,
                                     const string &_post, const string &_encoding) :
        chainid(_chainId),
        uri(_uri),
        jsps(_jsps),
        trims(_trims),
        requestTime(_time),
        post(_post),
        encoding(_encoding) {


    checkEncoding(_encoding);
    CHECK_STATE(uri.size() <= ORACLE_MAX_URI_SIZE);



    try {

        for (pow = 0;; pow++) {

            spec = "{";

            spec.append(string("\"cid\":") + to_string(chainid) + ",");
            spec.append(string("\"uri\":\"") + uri + "\",");
            spec.append(string("\"jsps\":["));

            for (uint64_t j = 0; j < jsps.size(); j++) {
                spec.append("\"");
                string jsp = jsps.at(j);
                CHECK_STATE(jsp.size() <= ORACLE_MAX_JSP_SIZE);
                CHECK_STATE2(!jsp.empty() && jsp.front() == '/', "Invalid JSP pointer:" + jsp);
                spec.append(jsp);
                spec.append("\"");
                if (j + 1 < jsps.size())
                    spec.append(",");
            }


            spec.append("],");

            if (trims.size() > 0) {

                CHECK_STATE(_trims.size() == _jsps.size());

                spec.append("\"trims\":[");

                for (uint64_t j = 0; j < trims.size(); j++) {
                    spec.append(to_string(trims.at(j)));
                    if (j + 1 < trims.size())
                        spec.append(",");
                }

                spec.append("],");
            }
            spec.append(string("\"time\":") + to_string(requestTime) + ",");

            if (!post.empty()) {
                spec.append(string("\"post\":\"") + post + "\",");
            }

            if (!encoding.empty()) {
                spec.append(string("\"encoding\":\"") + encoding + "\",");
            }

            spec.append(string("\"pow\":") + to_string(pow));
            spec.append("}");

            if (verifyPow()) {
                break;
            }


        }

        receipt = CryptoManager::hashForOracle(spec.data(), spec.size());

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}

const string &OracleRequestSpec::getEncoding() const {
    return encoding;
}


