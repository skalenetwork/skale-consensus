//
// Created by kladko on 10.01.22.
//

#ifndef CONSENSUS_ORACLERECEIVEDRESULTS_H
#define CONSENSUS_ORACLERECEIVEDRESULTS_H

class OracleResult;

class OracleRequestSpec;

class OracleReceivedResults {
    recursive_mutex m;
    uint64_t requiredConfirmations;
    uint64_t nodeCount;
    uint64_t requestTime;
    ptr<map<uint64_t, string>> signaturesBySchainIndex;
    ptr<map<string, uint64_t>> resultsByCount;
    ptr<OracleRequestSpec> requestSpec;
    bool isSgx;

    vector<uint8_t> ecdsaSigStringToByteArray(string& _sig );

public:

    const ptr<OracleRequestSpec> &getRequestSpec() const;

    OracleReceivedResults(ptr<OracleRequestSpec> _requestSpec, uint64_t _requiredSigners, uint64_t _nodeCount,
                          bool _isMockup);

    uint64_t getRequestTime() const;

    void insertIfDoesntExist(uint64_t _origin, ptr<OracleResult> _oracleResult);

    uint64_t tryGettingResult(string &_result);

    string compileCompleteResultJson(string &_unsignedResult);

    string compileCompleteResultRlp(string &_unsignedResult);

};


#endif //CONSENSUS_ORACLERECEIVEDRESULTS_H
