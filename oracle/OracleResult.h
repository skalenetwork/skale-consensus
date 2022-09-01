//
// Created by kladko on 14.01.22.
//

#ifndef SKALED_ORACLERESULT_H
#define SKALED_ORACLERESULT_H

#include "rlp/RLP.h"

class OracleRequestSpec;
class CryptoManager;



class OracleResult {

    string oracleResult;
    string encoding;


    uint64_t chainId;
    string uri;
    vector<string> jsps;
    vector<uint64_t> trims;
    uint64_t requestTime;
    string post;
    uint64_t error = 0;
    ptr<vector<ptr<string>>> results;
    ptr<vector<uint8_t>> rlp;
    string sig;


    RLPOutputStream stream;


    ptr<vector<ptr<string>>> extractResults(string &_response);


    void parseResultAsJson();
    void parseResultAsRlp();



    void trimResults();

    void appendElementsFromTheSpecAsJson();

    void appendResultsAsJson();

    void signResultAsJson(ptr<CryptoManager> _cryptoManager);

    void appendErrorAsJson();

    void encodeAndSignResultAsJson(ptr<CryptoManager> _cryptoManager);

    void encodeAndSignResultAsRlp(ptr<CryptoManager> _cryptoManager);

    void appendElementsFromTheSpecAsRlp();

    void appendResultsAsRlp();

    void signResultAsRlp(ptr<CryptoManager> _cryptoManager);

    void appendErrorAsRlp();


public:


    const string getUnsignedOracleResultStr() const;

    OracleResult(ptr<OracleRequestSpec> _spec, uint64_t _status, string& _serverResponse,
                 ptr<CryptoManager> _cryptoManager );


    OracleResult(string& _oracleResult, string& _encoding);

    static ptr<OracleResult> parseResult(string& _oracleResult, string& _encoding);

    const string &getSig() const;

    uint64_t getChainId() const;


    const vector<string> &getJsps() const;

    const string &toString() const;

    const string &getUri() const;

    uint64_t getTime() const;

    const vector<uint64_t> &getTrims() const;

    const string &getOracleResult() const;

    uint64_t getError() const;

    const ptr<vector<ptr<string>>> getResults() const;

    const string& getPost() const;

    bool isGeth();

};


#endif //SKALED_ORACLERESULT_H
