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
    uint64_t time;
    string pow;
    bool isPost = false;
    string postStr = "";

public:

    bool getPost() const;

    const string &getPostStr() const;


    const vector<string> &getJsps() const;

    const string &getSpec() const;

    const string &getUri() const;

    uint64_t getTime() const;

    const string &getPow() const;

    OracleRequestSpec(string& _spec);

    uint64_t getChainid() const;

    static ptr<OracleRequestSpec> parseSpec(string& _spec);

    const vector<uint64_t> &getTrims() const;

    bool isGeth();

    string getReceipt();

};


#endif //SKALED_ORACLEREQUESTSPEC_H
