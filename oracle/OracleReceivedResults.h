//
// Created by kladko on 10.01.22.
//

#ifndef CONSENSUS_ORACLERECEIVEDRESULTS_H
#define CONSENSUS_ORACLERECEIVEDRESULTS_H


class OracleReceivedResults {
    uint64_t requestTime;
public:
    OracleReceivedResults();

    uint64_t getRequestTime() const;


    ptr<map<uint64_t, string>> resultsBySchainIndex;
    ptr<map<string, uint64_t>> resultsByCount;
};


#endif //CONSENSUS_ORACLERECEIVEDRESULTS_H
