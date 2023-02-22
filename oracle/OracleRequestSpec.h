//
// Created by kladko on 11.01.22.
//


#ifndef SKALED_ORACLEREQUESTSPEC_H
#define SKALED_ORACLEREQUESTSPEC_H

#include "thirdparty/rapidjson/document.h"

constexpr const char* ORACLE_ENCODING_RLP = "rlp";
constexpr const char* ORACLE_ENCODING_JSON = "json";
constexpr const char* ORACLE_HTTP_START = "http://";
constexpr const char* ORACLE_HTTPS_START = "https://";
constexpr const char* ORACLE_ETH_URL = "eth://";

constexpr uint64_t ORACLE_MAX_URI_SIZE = 1024;
constexpr uint64_t ORACLE_MAX_POST_SIZE = 1024;
constexpr uint64_t ORACLE_MAX_JSPS = 32;
constexpr uint64_t ORACLE_MAX_JSP_SIZE = 1024;


class OracleRequestSpec {

    string spec;
    uint64_t chainid;
    string uri;
    vector<string> jsps;
    vector<uint64_t> trims;
    uint64_t requestTime;
    string post;
    string encoding;
    uint64_t pow;
    string receipt;
    string ethApi;


    void checkEncoding(const string & _encoding);
    void checkURI(const string & _uri);
    void checkEthApi(const string& _api);

    void  parseWebRequestSpec(rapidjson::Document& d, const string&  _spec);
    void  parseEthApiRequestSpec(rapidjson::Document& d, const string&  _spec);


public:


    const string &getPost() const;


    const vector<string> &getJsps() const;

    const string &getSpec() const;

    const string &getUri() const;

    uint64_t getTime() const;

    const uint64_t &getPow() const;

    OracleRequestSpec(const string &_spec);

    static ptr<OracleRequestSpec> makeSpec(uint64_t _chainId, const string &_uri, const vector<string> &_jsps,
                      const vector<uint64_t> &_trims, uint64_t _time, const string& _post, const string &_encoding);

    uint64_t getChainid() const;

    static ptr<OracleRequestSpec> parseSpec(const string &_spec, uint64_t _chainId);

    const vector<uint64_t> &getTrims() const;

    bool isEthApi();

    bool isPost();

    string getReceipt();

    static bool verifyPow(string& _spec);

    const string &getEncoding() const;

    const string &getEthApi() const;

    bool isEthMainnet() const;




};


#endif //SKALED_ORACLEREQUESTSPEC_H
