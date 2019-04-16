#pragma  once


#include "Header.h"

class NodeInfo;
class BlockProposal;
class Schain;
class Transaction;

class MissingTransactionsResponseHeader : public Header{

    ptr<vector<uint64_t>> missingTransactionSizes;

public:


    MissingTransactionsResponseHeader();

    MissingTransactionsResponseHeader(
            ptr<vector<uint64_t>> _missingTransactionSizes);

    void addFields(nlohmann::basic_json<> &_j) override;

};



