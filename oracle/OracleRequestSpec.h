//
// Created by kladko on 11.01.22.
//


#ifndef SKALED_ORACLEREQUESTSPEC_H
#define SKALED_ORACLEREQUESTSPEC_H

#include "thirdparty/rapidjson/document.h"

//constexpr const char* ORACLE_ENCODING_ABI = "abi";
constexpr const char *ORACLE_ENCODING_JSON = "json";
constexpr const char *ORACLE_HTTP_START = "http://";
constexpr const char *ORACLE_HTTPS_START = "https://";
constexpr const char *ORACLE_ETH_URL = "eth://";

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
    string from;
    string to;
    string data;
    string gas;
    string blockId;
    string encoding;
    string ethApi;
    uint64_t pow;
    string receipt;


    void checkEncoding(const string &_encoding);

    void checkURI(const string &_uri);

    void checkEthApi(const string &_api);

    void parseWebRequestSpec(rapidjson::Document &d, const string &_spec);

    void parseEthApiRequestSpec(rapidjson::Document &d, const string &_spec);

    static ptr<OracleRequestSpec> makeSpec(uint64_t _chainId, const string &_uri,
                                           const vector<string> &_jsps, const vector<uint64_t> &_trims,
                                           const string &_post,
                                           const string &_ethApi,
                                           const string &_from, const string &_to,
                                           const string &_data,
                                           const string &_gas,
                                           const string &_blockId,
                                           const string &_encoding, uint64_t _time);


    static string makeSpecStart(uint64_t _chainId, const string &_uri);

    static void appendSpecEnd(string &specStr, const string &_encoding, uint64_t _time, uint64_t _pow);

    static void
    appendWebPart(string &_specStr, const vector<string> &_jsps, const vector<uint64_t> &_trims, const string &_post);

    static string
    tryMakingSpec(uint64_t _chainId, const string &_uri, const vector<string> &_jsps, const vector<uint64_t> &_trims,
                  const string &_post,
                  const string &_ethApi, const string &_from, const string &_to, const string &_data,
                  const string &_gas,
                  const string &_blockId,
                  const string &_encoding, uint64_t _time, uint64_t _pow);

    static void
    appendEthCallPart(string &_specStr, const string &_from, const string &_to, const string &_data,
                      const string &_gas, const string &_blockId);

    static string checkAndGetParamsField(const rapidjson::GenericValue<rapidjson::UTF8<>>::Array &params,
                                  const string& _fieldName, const string& _spec);

    static bool isValidEthHexAddressString(const string &_address);


    static bool isHexEncodedUInt64(const string& _s);

    atomic<uint64_t> ethCallCounter = 0;


public:


    const string &getPost() const;


    const vector<string> &getJsps() const;

    const string &getSpec() const;

    const string &getUri() const;

    uint64_t getTime() const;

    const uint64_t &getPow() const;

    OracleRequestSpec(const string &_spec);


    uint64_t getChainid() const;

    static ptr<OracleRequestSpec> parseSpec(const string &_spec, uint64_t _chainId);

    const vector<uint64_t> &getTrims() const;

    bool isEthApi();

    bool isPost();

    string whatToPost();

    string createEthCallPostString();

    string getReceipt();

    static bool verifyPow(string &_spec);

    const string &getEncoding() const;

    const string &getEthApi() const;

    bool isEthMainnet() const;

    static ptr<OracleRequestSpec> makeWebSpec(uint64_t _chainId, const string &_uri,
                                              const vector<string> &_jsps, const vector<uint64_t> &_trims,
                                              const string &_post,
                                              const string &_encoding, uint64_t _time);

};


#endif //SKALED_ORACLEREQUESTSPEC_H
