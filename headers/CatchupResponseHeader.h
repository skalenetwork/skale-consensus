#pragma  once




#include "Header.h"

class NodeInfo;
class BlockProposal;
class Schain;


class Transaction;



class CatchupResponseHeader : public Header{
public:

    uint64_t getBlockCount() const;

    void setBlockCount(uint64_t blockCount);


private:
    uint64_t blockCount = 0;

    ptr<list<uint64_t>> blockSizes = nullptr;

public:

    CatchupResponseHeader();

    CatchupResponseHeader(ptr<list<uint64_t>>_blockSizes);

    void setBlockSizes(ptr<list<uint64_t>> _blockSizes);

    void addFields(nlohmann::basic_json<> &j_) override;

};



