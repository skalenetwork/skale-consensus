//
// Created by kladko on 11.01.22.
//

#ifndef SKALED_ORACLEREQUESTSPEC_H
#define SKALED_ORACLEREQUESTSPEC_H


class OracleRequestSpec {

    string url;
    string jsonPath;



public:

    OracleRequestSpec(string _spec);

    ptr<OracleRequestSpec> parseSpec(string& _spec);

};


#endif //SKALED_ORACLEREQUESTSPEC_H
