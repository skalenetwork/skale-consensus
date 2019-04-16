#pragma once

#include "BlockProposal.h"

class Schain;

class CommittedBlock : public  BlockProposal {


    uint64_t  headerSize = 0;
public:
    uint64_t getHeaderSize() const;

public:

    CommittedBlock(Schain& _sChain, ptr<BlockProposal> _p);

    CommittedBlock(ptr<vector<uint8_t>> _serializedBlock);

    ptr<vector<uint8_t>> serialize();

    ptr<vector<uint8_t>> serializedBlock = nullptr;

};