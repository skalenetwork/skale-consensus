//
// Created by kladko on 14.01.22.
//

#ifndef SKALED_ORACLERESULT_H
#define SKALED_ORACLERESULT_H


class OracleResult {
public:
    const string &getSig() const;

private:

    string oracleResult;
    string uri;

    vector<string> jsps;
    vector<uint64_t> trims;
    uint64_t time;
    uint64_t status = 0;
    uint64_t chainId;
    vector<ptr<string>> results;
    bool isPost;
    string postStr;

public:
    uint64_t getChainId() const;

private:
    string sig;
public:

    const vector<string> &getJsps() const;

    const string &getResult() const;

    const string &getUri() const;

    uint64_t getTime() const;

    OracleResult(string& _oracleResult);

    static ptr<OracleResult> parseResult(string& _oracleResult);

    const vector<uint64_t> &getTrims() const;

    const string &getOracleResult() const;

    uint64_t getStatus() const;

    const vector<ptr<string>> &getResults() const;

    const string &getPostStr() const;

    bool getPost() const;

    bool isGeth();

};


#endif //SKALED_ORACLERESULT_H
