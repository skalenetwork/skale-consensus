//
// Created by kladko on 14.01.22.
//

#ifndef SKALED_ORACLERESULT_H
#define SKALED_ORACLERESULT_H


class OracleRequestSpec;

class OracleResult {

    string oracleResult;


    uint64_t chainId;
    string uri;
    vector<string> jsps;
    vector<uint64_t> trims;
    uint64_t requestTime;
    string post;
    uint64_t error = 0;
    vector<ptr<string>> results;
    string sig;



    ptr<vector<ptr<string>>> extractResults(string &_response);

public:




    OracleResult(ptr<OracleRequestSpec> _spec, uint64_t _status, string& _serverResponse);


    OracleResult(string& _oracleResult);

    static ptr<OracleResult> parseResult(string& _oracleResult);

    const string &getSig() const;

    uint64_t getChainId() const;


    const vector<string> &getJsps() const;

    const string &getResult() const;

    const string &getUri() const;

    uint64_t getTime() const;

    const vector<uint64_t> &getTrims() const;

    const string &getOracleResult() const;

    uint64_t getError() const;

    const vector<ptr<string>> &getResults() const;

    const string& getPost() const;

    bool isGeth();

};


#endif //SKALED_ORACLERESULT_H
