#pragma  once



#include "Header.h"

class NodeInfo;
class BlockProposal;
class Schain;

class MissingTransactionsRequestHeader : public Header{


    uint64_t missingTransactionsCount;

public:


    MissingTransactionsRequestHeader();

    MissingTransactionsRequestHeader(ptr<map<uint64_t, ptr<partial_sha_hash>>> missingMessages_);

    void addFields(nlohmann::basic_json<> &j_) override;

    uint64_t getMissingTransactionsCount() const;

    void setMissingTransactionsCount(uint64_t missingTransactionsCount);

};



