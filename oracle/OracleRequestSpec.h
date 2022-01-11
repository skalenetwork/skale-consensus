//
// Created by kladko on 11.01.22.
//

#ifndef SKALED_ORACLEREQUESTSPEC_H
#define SKALED_ORACLEREQUESTSPEC_H


class OracleRequestSpec {

    string url;
    string jsonPath;

    OracleRequestSpec(string _spec);

public:

    ptr<OracleRequestSpec> parseSpec();

};


#endif //SKALED_ORACLEREQUESTSPEC_H
