//
// Created by kladko on 11.01.22.
//

#ifndef SKALED_ORACLEREQUESTSPEC_H
#define SKALED_ORACLEREQUESTSPEC_H


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


public:

    bool getPost() const;

    const string &getPostStr() const;


    const vector<string> &getJsps() const;

    const string &getSpec() const;

    const string &getUri() const;

    uint64_t getTime() const;

    const uint64_t &getPow() const;

    OracleRequestSpec(const string& _spec);

    OracleRequestSpec(uint64_t chainid, const string &uri, const vector<string> &jsps,
                      const vector<uint64_t> &trims, uint64_t time, const string &postStr, const string &encoding);

    uint64_t getChainid() const;

    static ptr<OracleRequestSpec> parseSpec(const string& _spec);

    const vector<uint64_t> &getTrims() const;

    bool isGeth();

    string getReceipt();

    bool verifyPow();

};


#endif //SKALED_ORACLEREQUESTSPEC_H
