//
// Created by kladko on 11.01.22.
//

#ifndef SKALED_ORACLEREQUESTSPEC_H
#define SKALED_ORACLEREQUESTSPEC_H


class OracleRequestSpec {

    string spec;
    string uri;

    vector<string> jsps;
    vector<uint64_t> trims;
    uint64_t time;
    string pow;

public:

    const vector<string> &getJsps() const;

    const string &getSpec() const;

    const string &getUri() const;

    uint64_t getTime() const;

    const string &getPow() const;

    OracleRequestSpec(string& _spec);

    static ptr<OracleRequestSpec> parseSpec(string& _spec);

    const vector<uint64_t> &getTrims() const;

};


#endif //SKALED_ORACLEREQUESTSPEC_H
