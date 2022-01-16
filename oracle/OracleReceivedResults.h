//
// Created by kladko on 10.01.22.
//

#ifndef CONSENSUS_ORACLERECEIVEDRESULTS_H
#define CONSENSUS_ORACLERECEIVEDRESULTS_H


class OracleReceivedResults {
    recursive_mutex m;
    uint64_t requiredConfirmations;
    uint64_t requestTime;
    ptr<map<uint64_t, string>> signaturesBySchainIndex;
    ptr<map<string, uint64_t>> resultsByCount;

public:

    OracleReceivedResults(uint64_t _requiredSigners);

    uint64_t getRequestTime() const;

    void insertIfDoesntExist(uint64_t _origin, string _unsignedResult, string _sig);

    uint64_t tryGettingResult(string& _result);


};


#endif //CONSENSUS_ORACLERECEIVEDRESULTS_H
