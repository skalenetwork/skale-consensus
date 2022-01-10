//
// Created by kladko on 10.01.22.
//

#ifndef CONSENSUS_ORACLERECEIVEDRESULTS_H
#define CONSENSUS_ORACLERECEIVEDRESULTS_H


class OracleReceivedResults {
public:
    OracleReceivedResults();

    ptr<map<uint64_t, string>> resultsBySchainIndex;
};


#endif //CONSENSUS_ORACLERECEIVEDRESULTS_H
