//
// Created by kladko on 11.01.22.
//

#ifndef SKALED_ORACLEREQUESTSPEC_H
#define SKALED_ORACLEREQUESTSPEC_H


class OracleRequestSpec {

    string spec;
    string uri;
    string jsp;
    uint64_t time;

public:

    OracleRequestSpec(string& _spec);

    static ptr<OracleRequestSpec> parseSpec(string& _spec);

};


#endif //SKALED_ORACLEREQUESTSPEC_H
