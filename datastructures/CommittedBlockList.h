#pragma once

#include "DataStructure.h"




class CommittedBlock;

class CommittedBlockList : public DataStructure {


    ptr<vector<ptr<CommittedBlock>>> blocks = nullptr;

public:


    CommittedBlockList(ptr<vector<size_t>> _blockSizes, ptr<vector<uint8_t>> _serializedBlocks);

    CommittedBlockList(ptr<vector<ptr<CommittedBlock>>> _blocks);

    ptr<vector<ptr<CommittedBlock>>> getBlocks() ;

    shared_ptr<vector<uint8_t>> serialize() ;

};



